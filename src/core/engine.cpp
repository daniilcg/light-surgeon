#include "lightsurgeon/engine.hpp"

#include "lightsurgeon/bvh.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <unordered_set>

namespace lightsurgeon {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Basis {
    Vec3 forward;
    Vec3 right;
    Vec3 up;
};

Basis cameraBasis(const CameraDesc& cam) {
    Basis b;
    b.forward = normalize(cam.aim - cam.position);
    if (length(b.forward) < 1e-12) {
        b.forward = {0.0, 0.0, -1.0};
    }
    b.right = normalize(cross(b.forward, cam.up));
    if (length(b.right) < 1e-12) {
        b.right = normalize(cross(b.forward, Vec3{0.0, 1.0, 0.0}));
        if (length(b.right) < 1e-12) {
            b.right = {1.0, 0.0, 0.0};
        }
    }
    b.up = normalize(cross(b.right, b.forward));
    return b;
}

Ray cameraRay(const CameraDesc& cam, const Basis& basis, double nx, double ny) {
    const double fov = cam.fovYDegrees * kPi / 180.0;
    const double h = std::tan(fov * 0.5);
    const double w = h * cam.aspect;
    const Vec3 dir = normalize(basis.forward + basis.right * ((nx * 2.0 - 1.0) * w) + basis.up * ((1.0 - ny) * 2.0 - 1.0) * h);
    Ray ray;
    ray.origin = cam.position;
    ray.direction = dir;
    ray.tMin = cam.nearClip;
    ray.tMax = cam.farClip;
    return ray;
}

bool inFrustum(const CameraDesc& cam, const Basis& basis, const Vec3& p) {
    const Vec3 rel = p - cam.position;
    const double z = dot(rel, basis.forward);
    if (z < cam.nearClip || z > cam.farClip) return false;
    const double fov = cam.fovYDegrees * kPi / 180.0;
    const double h = std::tan(fov * 0.5) * z;
    const double w = h * cam.aspect;
    const double x = dot(rel, basis.right);
    const double y = dot(rel, basis.up);
    return std::abs(x) <= w * 1.15 && std::abs(y) <= h * 1.15;
}

double effectiveIntensity(const LightDesc& light) {
    return std::max(0.0, light.intensity) * std::pow(2.0, light.exposure) * std::max(0.0, luma(light.color));
}

bool linkingAllows(const LightDesc& light, const std::string& meshName) {
    if (!light.includeObjects.empty()) {
        return std::find(light.includeObjects.begin(), light.includeObjects.end(), meshName) != light.includeObjects.end();
    }
    if (!light.excludeObjects.empty()) {
        return std::find(light.excludeObjects.begin(), light.excludeObjects.end(), meshName) == light.excludeObjects.end();
    }
    return true;
}

Vec3 lightPositionSample(const LightDesc& light, int sample, int sampleCount) {
    if (light.type != LightType::Area) {
        return light.position;
    }
    Vec3 n = normalize(light.direction);
    if (length(n) < 1e-12) n = {0.0, -1.0, 0.0};
    Vec3 t = normalize(cross(std::abs(n.y) < 0.9 ? Vec3{0.0, 1.0, 0.0} : Vec3{1.0, 0.0, 0.0}, n));
    Vec3 b = cross(n, t);
    if (sampleCount <= 1) return light.position;
    const int grid = std::max(2, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(sampleCount)))));
    const int sx = sample % grid;
    const int sy = (sample / grid) % grid;
    const double u = ((sx + 0.5) / grid - 0.5) * light.areaWidth;
    const double v = ((sy + 0.5) / grid - 0.5) * light.areaHeight;
    return light.position + t * u + b * v;
}

double evaluateLight(const LightDesc& light, const Hit& hit, const std::string& meshName, const Bvh& bvh,
                     const AnalyzeSettings& settings) {
    if (!light.enabled) return 0.0;
    if (!linkingAllows(light, meshName)) return 0.0;

    Vec3 toLight;
    double distance = 1.0;
    double attenuation = 1.0;
    double nDotL = 1.0;

    if (light.type == LightType::Dome) {
        const Vec3 up = normalize(-light.direction);
        const Vec3 ldir = length(up) < 1e-12 ? Vec3{0.0, 1.0, 0.0} : up;
        nDotL = std::max(0.0, dot(hit.normal, ldir) * 0.5 + 0.5);
        Ray shadow;
        shadow.origin = hit.point + hit.normal * settings.shadowBias;
        shadow.direction = ldir;
        shadow.tMin = settings.shadowBias;
        shadow.tMax = 1e5;
        if (bvh.occluded(shadow)) return 0.0;
        return effectiveIntensity(light) * nDotL * 0.18;
    }

    if (light.type == LightType::Directional) {
        toLight = normalize(-light.direction);
        if (length(toLight) < 1e-12) toLight = {0.0, 1.0, 0.0};
        nDotL = std::max(0.0, dot(hit.normal, toLight));
        if (nDotL <= 0.0) return 0.0;
        Ray shadow;
        shadow.origin = hit.point + hit.normal * settings.shadowBias;
        shadow.direction = toLight;
        shadow.tMin = settings.shadowBias;
        shadow.tMax = 1e5;
        if (bvh.occluded(shadow)) return 0.0;
        return effectiveIntensity(light) * nDotL;
    }

    double vis = 0.0;
    const int samples = light.type == LightType::Area ? std::max(1, settings.areaSamples) : 1;
    for (int s = 0; s < samples; ++s) {
        const Vec3 lp = lightPositionSample(light, s, samples);
        Vec3 lvec = lp - hit.point;
        distance = length(lvec);
        if (distance < 1e-8) {
            vis += 1.0;
            continue;
        }
        toLight = lvec / distance;
        nDotL = std::max(0.0, dot(hit.normal, toLight));
        if (nDotL <= 0.0) continue;
        if (light.type == LightType::Spot) {
            const Vec3 spotDir = normalize(light.direction);
            const double angle = std::acos(std::max(-1.0, std::min(1.0, dot(-toLight, spotDir))));
            const double outer = (light.coneAngleDegrees * 0.5) * kPi / 180.0;
            const double inner = std::max(0.0, outer - light.penumbraDegrees * kPi / 180.0);
            if (angle > outer) continue;
            double spot = 1.0;
            if (angle > inner && outer > inner) {
                spot = 1.0 - (angle - inner) / (outer - inner);
            }
            nDotL *= spot;
        }
        Ray shadow;
        shadow.origin = hit.point + hit.normal * settings.shadowBias;
        shadow.direction = toLight;
        shadow.tMin = settings.shadowBias;
        shadow.tMax = std::max(settings.shadowBias * 2.0, distance - settings.shadowBias);
        if (bvh.occluded(shadow)) continue;
        vis += nDotL;
    }
    vis /= static_cast<double>(samples);
    if (vis <= 0.0) return 0.0;
    attenuation = 1.0 / std::max(distance * distance, 1e-4);
    if (light.type == LightType::Area) {
        const double area = std::max(1e-4, light.areaWidth * light.areaHeight);
        attenuation *= area;
    }
    return effectiveIntensity(light) * vis * attenuation;
}

Bvh buildSceneBvh(const SceneDesc& scene, const AnalyzeSettings& settings, std::vector<std::string>* meshNames) {
    std::vector<Triangle> tris;
    tris.reserve(65536);
    meshNames->clear();
    int triBudget = 0;
    for (std::size_t mi = 0; mi < scene.meshes.size(); ++mi) {
        const MeshDesc& mesh = scene.meshes[mi];
        meshNames->push_back(mesh.name);
        const int nTri = static_cast<int>(mesh.triangles.size() / 3);
        for (int t = 0; t < nTri; ++t) {
            if (triBudget >= settings.maxTriangles) break;
            const int i0 = mesh.triangles[static_cast<std::size_t>(t * 3 + 0)];
            const int i1 = mesh.triangles[static_cast<std::size_t>(t * 3 + 1)];
            const int i2 = mesh.triangles[static_cast<std::size_t>(t * 3 + 2)];
            if (i0 < 0 || i1 < 0 || i2 < 0) continue;
            if (i0 >= static_cast<int>(mesh.vertices.size()) || i1 >= static_cast<int>(mesh.vertices.size()) ||
                i2 >= static_cast<int>(mesh.vertices.size())) {
                continue;
            }
            Triangle tri;
            tri.v0 = mesh.vertices[static_cast<std::size_t>(i0)];
            tri.v1 = mesh.vertices[static_cast<std::size_t>(i1)];
            tri.v2 = mesh.vertices[static_cast<std::size_t>(i2)];
            tri.meshIndex = static_cast<int>(mi);
            tri.triIndex = t;
            if (tri.area() <= 1e-12) continue;
            tris.push_back(tri);
            ++triBudget;
        }
    }
    Bvh bvh;
    bvh.build(std::move(tris));
    return bvh;
}

std::string classifyRole(const LightDesc& light, const CameraDesc& cam, const Basis& basis, double energy,
                         double bestEnergy) {
    if (energy <= 1e-12) return "dead";
    if (light.type == LightType::Dome) return "environment";
    Vec3 incoming;
    if (light.type == LightType::Directional) {
        incoming = normalize(-light.direction);
    } else {
        incoming = normalize(light.position - cam.position);
    }
    const double camZ = dot(incoming, basis.forward);
    const double camX = dot(incoming, basis.right);
    const double camY = dot(incoming, basis.up);
    if (camZ < -0.25 && energy > bestEnergy * 0.08) return "rim";
    if (energy >= bestEnergy * 0.72 && camY > -0.2) return "key";
    if (camX < 0.0 && energy >= bestEnergy * 0.12) return "fill";
    if (camY < -0.35) return "bounce";
    return "utility";
}

std::vector<PortalCandidate> findPortals(const Bvh& bvh, const SceneDesc& scene, const std::vector<std::string>& meshNames) {
    std::vector<PortalCandidate> portals;
    if (bvh.triangleCount() == 0) return portals;
    for (std::size_t i = 0; i < bvh.triangleCount(); ++i) {
        const Triangle& tri = bvh.triangle(i);
        const Vec3 n = tri.geometricNormal();
        const Vec3 c = tri.centroid();
        const double area = tri.area();
        if (area < 0.05) continue;
        for (const LightDesc& light : scene.lights) {
            if (!light.enabled) continue;
            if (light.type != LightType::Directional && light.type != LightType::Dome && light.type != LightType::Area) {
                continue;
            }
            Vec3 ldir = light.type == LightType::Directional || light.type == LightType::Dome
                            ? normalize(light.direction)
                            : normalize(c - light.position);
            if (length(ldir) < 1e-12) continue;
            // Light hitting the backface of a thin opening facing the interior.
            if (dot(n, ldir) > 0.35) {
                Vec3 toCam = scene.camera.position - c;
                if (dot(n, toCam) > 0.0) {
                    PortalCandidate p;
                    p.meshName = (tri.meshIndex >= 0 && tri.meshIndex < static_cast<int>(meshNames.size()))
                                     ? meshNames[static_cast<std::size_t>(tri.meshIndex)]
                                     : "";
                    p.center = c;
                    p.normal = n;
                    p.area = area;
                    p.drivenByLight = light.name;
                    p.score = area * effectiveIntensity(light) * std::abs(dot(n, ldir));
                    portals.push_back(p);
                }
            }
        }
    }
    std::sort(portals.begin(), portals.end(), [](const PortalCandidate& a, const PortalCandidate& b) {
        return a.score > b.score;
    });
    if (portals.size() > 12) portals.resize(12);
    return portals;
}

std::vector<LeakHit> findLeaks(const SceneDesc& scene, const Bvh& bvh, const Basis& basis,
                               const AnalyzeSettings& settings, const std::vector<std::string>& meshNames) {
    std::vector<LeakHit> leaks;
    const CameraDesc& cam = scene.camera;
    for (int i = 0; i < settings.leakSamples; ++i) {
        const double nx = (i % 8 + 0.5) / 8.0;
        const double ny = (i / 8 + 0.5) / 6.0;
        Ray ray = cameraRay(cam, basis, nx, std::min(0.999, ny));
        const Hit hit = bvh.closestHit(ray);
        if (hit.hit) continue;
        int meshIndex = -1;
        const double gap = bvh.closestApproach(ray, &meshIndex);
        if (gap > settings.leakGap || gap >= 1e20) continue;
        for (const LightDesc& light : scene.lights) {
            if (!light.enabled) continue;
            Vec3 ldir;
            double tmax = 1e5;
            if (light.type == LightType::Directional || light.type == LightType::Dome) {
                ldir = normalize(-light.direction);
            } else {
                Vec3 v = light.position - cam.position;
                tmax = length(v);
                if (tmax < 1e-6) continue;
                ldir = v / tmax;
            }
            Ray lr;
            lr.origin = cam.position;
            lr.direction = ldir;
            lr.tMin = cam.nearClip;
            lr.tMax = tmax;
            if (!bvh.occluded(lr)) {
                LeakHit leak;
                leak.origin = cam.position;
                leak.direction = ray.direction;
                leak.gap = gap;
                leak.nearMesh = (meshIndex >= 0 && meshIndex < static_cast<int>(meshNames.size()))
                                    ? meshNames[static_cast<std::size_t>(meshIndex)]
                                    : "";
                leak.lightName = light.name;
                leaks.push_back(leak);
                break;
            }
        }
    }
    return leaks;
}

}  // namespace

AnalysisResult analyzeScene(const SceneDesc& scene, const AnalyzeSettings& settings) {
    AnalysisResult result;
    result.frame = scene.frame;
    std::vector<std::string> meshNames;
    const Bvh bvh = buildSceneBvh(scene, settings, &meshNames);
    const Basis basis = cameraBasis(scene.camera);
    const int sw = std::max(4, settings.sampleWidth);
    const int sh = std::max(4, settings.sampleHeight);
    result.samples = sw * sh;

    result.lights.resize(scene.lights.size());
    for (std::size_t i = 0; i < scene.lights.size(); ++i) {
        result.lights[i].name = scene.lights[i].name;
        result.lights[i].type = scene.lights[i].type;
        if (scene.lights[i].type != LightType::Directional && scene.lights[i].type != LightType::Dome) {
            result.lights[i].outsideFrustum = !inFrustum(scene.camera, basis, scene.lights[i].position);
        }
    }

    for (int y = 0; y < sh; ++y) {
        for (int x = 0; x < sw; ++x) {
            const double nx = (x + 0.5) / sw;
            const double ny = (y + 0.5) / sh;
            Ray ray = cameraRay(scene.camera, basis, nx, ny);
            const Hit hit = bvh.closestHit(ray);
            if (!hit.hit) continue;
            ++result.hits;
            const std::string& meshName = meshNames[static_cast<std::size_t>(hit.meshIndex)];
            for (std::size_t li = 0; li < scene.lights.size(); ++li) {
                const double e = evaluateLight(scene.lights[li], hit, meshName, bvh, settings);
                if (e > 0.0) {
                    result.lights[li].energy += e;
                    result.lights[li].maxSample = std::max(result.lights[li].maxSample, e);
                    ++result.lights[li].litSamples;
                }
            }
        }
    }

    double bestEnergy = 0.0;
    for (auto& ls : result.lights) {
        result.totalEnergy += ls.energy;
        bestEnergy = std::max(bestEnergy, ls.energy);
        ls.pixelFraction = result.samples > 0 ? static_cast<double>(ls.litSamples) / result.samples : 0.0;
    }

    for (std::size_t i = 0; i < scene.lights.size(); ++i) {
        LightStats& ls = result.lights[i];
        const LightDesc& light = scene.lights[i];
        ls.dead = light.enabled && ls.energy <= settings.deadEnergyEpsilon;
        const double radius = light.type == LightType::Area ? 0.5 * std::min(light.areaWidth, light.areaHeight) : light.radius;
        const double area = std::max(1e-6, kPi * radius * radius);
        const double density = effectiveIntensity(light) / area;
        ls.noiseScore = density * (ls.pixelFraction > 0.0 ? (1.0 - std::min(1.0, ls.pixelFraction / 0.2)) : 1.0);
        ls.noisy = light.enabled && !ls.dead && radius <= settings.noisyRadius && density >= settings.noisyEnergyDensity;
        if (!ls.noisy && light.enabled && !ls.dead && ls.pixelFraction > 0.0 &&
            ls.pixelFraction <= settings.noisyPixelFraction && density >= settings.noisyEnergyDensity * 0.5) {
            ls.noisy = true;
        }
        if (!light.includeObjects.empty()) {
            std::unordered_set<std::string> names(meshNames.begin(), meshNames.end());
            bool any = false;
            for (const auto& inc : light.includeObjects) {
                if (names.count(inc)) any = true;
            }
            ls.linkingEmpty = !any;
        }
        ls.role = classifyRole(light, scene.camera, basis, ls.energy, bestEnergy);
    }

    std::sort(result.lights.begin(), result.lights.end(), [](const LightStats& a, const LightStats& b) {
        return a.energy > b.energy;
    });
    result.portals = findPortals(bvh, scene, meshNames);
    result.leaks = findLeaks(scene, bvh, basis, settings, meshNames);
    return result;
}

PixelAutopsy autopsyPixel(const SceneDesc& scene, int x, int y, const AnalyzeSettings& settings) {
    PixelAutopsy out;
    out.x = x;
    out.y = y;
    std::vector<std::string> meshNames;
    const Bvh bvh = buildSceneBvh(scene, settings, &meshNames);
    const Basis basis = cameraBasis(scene.camera);
    const double nx = (static_cast<double>(x) + 0.5) / std::max(1, scene.camera.width);
    const double ny = (static_cast<double>(y) + 0.5) / std::max(1, scene.camera.height);
    Ray ray = cameraRay(scene.camera, basis, nx, ny);
    const Hit hit = bvh.closestHit(ray);
    if (!hit.hit) return out;
    out.hit = true;
    out.point = hit.point;
    out.normal = hit.normal;
    out.meshName = meshNames[static_cast<std::size_t>(hit.meshIndex)];
    double total = 0.0;
    for (const LightDesc& light : scene.lights) {
        Contribution c;
        c.name = light.name;
        c.irradiance = evaluateLight(light, hit, out.meshName, bvh, settings);
        c.shadowed = light.enabled && c.irradiance <= 0.0;
        total += std::max(0.0, c.irradiance);
        out.lights.push_back(c);
    }
    std::sort(out.lights.begin(), out.lights.end(), [](const Contribution& a, const Contribution& b) {
        return a.irradiance > b.irradiance;
    });
    if (total > 0.0) {
        for (auto& c : out.lights) c.fraction = c.irradiance / total;
    }
    return out;
}

MatchResult matchHero(const AnalysisResult& current, const AnalysisResult& hero) {
    MatchResult match;
    auto energyByRole = [](const AnalysisResult& a, const std::string& role) {
        double e = 0.0;
        for (const auto& l : a.lights) {
            if (l.role == role) e += l.energy;
        }
        return e;
    };
    const std::string roles[] = {"key", "fill", "rim", "environment", "bounce", "utility"};
    double heroTotal = std::max(1e-12, hero.totalEnergy);
    double curTotal = std::max(1e-12, current.totalEnergy);
    match.exposureDeltaStops = std::log2(heroTotal / curTotal);

    for (const auto& role : roles) {
        const double h = energyByRole(hero, role);
        const double c = energyByRole(current, role);
        if (h <= 1e-12 && c <= 1e-12) continue;
        MatchScale s;
        s.role = role;
        s.name = role;
        s.heroEnergy = h;
        s.currentEnergy = c;
        s.intensityScale = c <= 1e-12 ? (h > 0.0 ? 2.0 : 1.0) : std::max(0.05, std::min(8.0, h / c));
        for (const auto& l : current.lights) {
            if (l.role == role) {
                s.name = l.name;
                break;
            }
        }
        match.scales.push_back(s);
    }
    return match;
}

AnalysisResult mergeAnalyses(const std::vector<AnalysisResult>& parts) {
    AnalysisResult out;
    if (parts.empty()) return out;
    out.frame = parts.front().frame;
    std::map<std::string, LightStats> byName;
    for (const auto& part : parts) {
        out.samples += part.samples;
        out.hits += part.hits;
        out.totalEnergy += part.totalEnergy;
        if (part.frame > out.frame) out.frame = part.frame;
        for (const auto& l : part.lights) {
            LightStats& acc = byName[l.name];
            if (acc.name.empty()) acc = l;
            else {
                acc.energy += l.energy;
                acc.maxSample = std::max(acc.maxSample, l.maxSample);
                acc.litSamples += l.litSamples;
                acc.noiseScore = std::max(acc.noiseScore, l.noiseScore);
                acc.dead = acc.dead && l.dead;
                acc.noisy = acc.noisy || l.noisy;
                acc.outsideFrustum = acc.outsideFrustum && l.outsideFrustum;
                acc.linkingEmpty = acc.linkingEmpty || l.linkingEmpty;
            }
        }
        if (out.portals.empty()) out.portals = part.portals;
        if (out.leaks.empty()) out.leaks = part.leaks;
    }
    for (auto& kv : byName) {
        kv.second.pixelFraction = out.samples > 0 ? static_cast<double>(kv.second.litSamples) / out.samples : 0.0;
        out.lights.push_back(kv.second);
    }
    std::sort(out.lights.begin(), out.lights.end(), [](const LightStats& a, const LightStats& b) {
        return a.energy > b.energy;
    });
    return out;
}

std::string formatReport(const AnalysisResult& result) {
    std::ostringstream os;
    os.precision(4);
    os << std::fixed;
    os << "Light Surgeon  frame " << result.frame << "  samples " << result.samples << "  hits " << result.hits
       << "  energy " << result.totalEnergy << "\n";
    os << "LIGHT                 ROLE          ENERGY     PIX%   NOISE  FLAGS\n";
    for (const auto& l : result.lights) {
        os << l.name;
        if (l.name.size() < 20) os << std::string(20 - l.name.size(), ' ');
        os << "  " << l.role;
        if (l.role.size() < 10) os << std::string(10 - l.role.size(), ' ');
        os << "  " << l.energy << "  " << (l.pixelFraction * 100.0) << "  " << l.noiseScore << "  ";
        if (l.dead) os << "DEAD ";
        if (l.noisy) os << "NOISY ";
        if (l.outsideFrustum) os << "OUTSIDE ";
        if (l.linkingEmpty) os << "LINK ";
        os << "\n";
    }
    os << "Portals: " << result.portals.size() << "  Leaks: " << result.leaks.size() << "\n";
    for (const auto& p : result.portals) {
        os << "  portal " << p.meshName << " score " << p.score << " light " << p.drivenByLight << "\n";
    }
    for (const auto& leak : result.leaks) {
        os << "  leak near " << leak.nearMesh << " gap " << leak.gap << " light " << leak.lightName << "\n";
    }
    return os.str();
}

}  // namespace lightsurgeon
