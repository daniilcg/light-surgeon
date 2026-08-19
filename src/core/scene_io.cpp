#include "lightsurgeon/scene_io.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace lightsurgeon {
namespace {

Vec3 vecFromJson(const Json& v) {
    const auto& a = v.asArray();
    if (a.size() != 3) throw std::runtime_error("Expected vec3 array");
    return {a[0].asNumber(), a[1].asNumber(), a[2].asNumber()};
}

Json vecToJson(const Vec3& v) {
    Json a = Json::array();
    a.push(v.x);
    a.push(v.y);
    a.push(v.z);
    return a;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void writeFile(const std::string& path, const std::string& data) {
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write file: " + path);
    out << data;
}

Json stringArray(const std::vector<std::string>& v) {
    Json a = Json::array();
    for (const auto& s : v) a.push(s);
    return a;
}

std::vector<std::string> parseStringArray(const Json& v) {
    std::vector<std::string> out;
    if (!v.isArray()) return out;
    for (const auto& i : v.asArray()) out.push_back(i.asString());
    return out;
}

}  // namespace

SceneDesc sceneFromJson(const Json& root) {
    SceneDesc scene;
    if (root.has("frame")) scene.frame = static_cast<int>(root.at("frame").asNumber());
    if (root.has("camera")) {
        const Json& c = root.at("camera");
        if (c.has("name")) scene.camera.name = c.at("name").asString();
        if (c.has("position")) scene.camera.position = vecFromJson(c.at("position"));
        if (c.has("aim")) scene.camera.aim = vecFromJson(c.at("aim"));
        if (c.has("up")) scene.camera.up = vecFromJson(c.at("up"));
        if (c.has("fov")) scene.camera.fovYDegrees = c.at("fov").asNumber();
        if (c.has("near")) scene.camera.nearClip = c.at("near").asNumber();
        if (c.has("far")) scene.camera.farClip = c.at("far").asNumber();
        if (c.has("aspect")) scene.camera.aspect = c.at("aspect").asNumber();
        if (c.has("width")) scene.camera.width = static_cast<int>(c.at("width").asNumber());
        if (c.has("height")) scene.camera.height = static_cast<int>(c.at("height").asNumber());
    }
    if (root.has("lights")) {
        for (const auto& j : root.at("lights").asArray()) {
            LightDesc l;
            l.name = j.has("name") ? j.at("name").asString() : "light";
            l.type = j.has("type") ? lightTypeFromName(j.at("type").asString()) : LightType::Point;
            if (j.has("position")) l.position = vecFromJson(j.at("position"));
            if (j.has("direction")) l.direction = vecFromJson(j.at("direction"));
            if (j.has("color")) l.color = vecFromJson(j.at("color"));
            if (j.has("intensity")) l.intensity = j.at("intensity").asNumber();
            if (j.has("exposure")) l.exposure = j.at("exposure").asNumber();
            if (j.has("radius")) l.radius = j.at("radius").asNumber();
            if (j.has("coneAngle")) l.coneAngleDegrees = j.at("coneAngle").asNumber();
            if (j.has("penumbra")) l.penumbraDegrees = j.at("penumbra").asNumber();
            if (j.has("areaWidth")) l.areaWidth = j.at("areaWidth").asNumber();
            if (j.has("areaHeight")) l.areaHeight = j.at("areaHeight").asNumber();
            if (j.has("enabled")) l.enabled = j.at("enabled").asBool();
            if (j.has("include")) l.includeObjects = parseStringArray(j.at("include"));
            if (j.has("exclude")) l.excludeObjects = parseStringArray(j.at("exclude"));
            scene.lights.push_back(l);
        }
    }
    if (root.has("meshes")) {
        for (const auto& j : root.at("meshes").asArray()) {
            MeshDesc m;
            m.name = j.has("name") ? j.at("name").asString() : "mesh";
            if (j.has("vertices")) {
                for (const auto& v : j.at("vertices").asArray()) m.vertices.push_back(vecFromJson(v));
            }
            if (j.has("triangles")) {
                for (const auto& t : j.at("triangles").asArray()) {
                    if (t.isNumber()) {
                        m.triangles.push_back(static_cast<int>(t.asNumber()));
                    } else {
                        const auto& trip = t.asArray();
                        if (trip.size() != 3) throw std::runtime_error("Triangle must have 3 indices");
                        m.triangles.push_back(static_cast<int>(trip[0].asNumber()));
                        m.triangles.push_back(static_cast<int>(trip[1].asNumber()));
                        m.triangles.push_back(static_cast<int>(trip[2].asNumber()));
                    }
                }
            }
            scene.meshes.push_back(std::move(m));
        }
    }
    return scene;
}

Json sceneToJson(const SceneDesc& scene) {
    Json root = Json::object();
    root["frame"] = scene.frame;
    Json cam = Json::object();
    cam["name"] = scene.camera.name;
    cam["position"] = vecToJson(scene.camera.position);
    cam["aim"] = vecToJson(scene.camera.aim);
    cam["up"] = vecToJson(scene.camera.up);
    cam["fov"] = scene.camera.fovYDegrees;
    cam["near"] = scene.camera.nearClip;
    cam["far"] = scene.camera.farClip;
    cam["aspect"] = scene.camera.aspect;
    cam["width"] = scene.camera.width;
    cam["height"] = scene.camera.height;
    root["camera"] = cam;
    Json lights = Json::array();
    for (const auto& l : scene.lights) {
        Json j = Json::object();
        j["name"] = l.name;
        j["type"] = lightTypeName(l.type);
        j["position"] = vecToJson(l.position);
        j["direction"] = vecToJson(l.direction);
        j["color"] = vecToJson(l.color);
        j["intensity"] = l.intensity;
        j["exposure"] = l.exposure;
        j["radius"] = l.radius;
        j["coneAngle"] = l.coneAngleDegrees;
        j["penumbra"] = l.penumbraDegrees;
        j["areaWidth"] = l.areaWidth;
        j["areaHeight"] = l.areaHeight;
        j["enabled"] = l.enabled;
        j["include"] = stringArray(l.includeObjects);
        j["exclude"] = stringArray(l.excludeObjects);
        lights.push(j);
    }
    root["lights"] = lights;
    Json meshes = Json::array();
    for (const auto& m : scene.meshes) {
        Json j = Json::object();
        j["name"] = m.name;
        Json verts = Json::array();
        for (const auto& v : m.vertices) verts.push(vecToJson(v));
        j["vertices"] = verts;
        Json tris = Json::array();
        for (std::size_t i = 0; i + 2 < m.triangles.size(); i += 3) {
            Json t = Json::array();
            t.push(m.triangles[i]);
            t.push(m.triangles[i + 1]);
            t.push(m.triangles[i + 2]);
            tris.push(t);
        }
        j["triangles"] = tris;
        meshes.push(j);
    }
    root["meshes"] = meshes;
    return root;
}

SceneDesc loadSceneFile(const std::string& path) { return sceneFromJson(Json::parse(readFile(path))); }

void saveSceneFile(const std::string& path, const SceneDesc& scene) { writeFile(path, sceneToJson(scene).dump(2)); }

Json analysisToJson(const AnalysisResult& result) {
    Json root = Json::object();
    root["frame"] = result.frame;
    root["samples"] = result.samples;
    root["hits"] = result.hits;
    root["totalEnergy"] = result.totalEnergy;
    Json lights = Json::array();
    for (const auto& l : result.lights) {
        Json j = Json::object();
        j["name"] = l.name;
        j["type"] = lightTypeName(l.type);
        j["energy"] = l.energy;
        j["maxSample"] = l.maxSample;
        j["litSamples"] = l.litSamples;
        j["pixelFraction"] = l.pixelFraction;
        j["noiseScore"] = l.noiseScore;
        j["dead"] = l.dead;
        j["noisy"] = l.noisy;
        j["outsideFrustum"] = l.outsideFrustum;
        j["linkingEmpty"] = l.linkingEmpty;
        j["role"] = l.role;
        lights.push(j);
    }
    root["lights"] = lights;
    Json portals = Json::array();
    for (const auto& p : result.portals) {
        Json j = Json::object();
        j["mesh"] = p.meshName;
        j["center"] = vecToJson(p.center);
        j["normal"] = vecToJson(p.normal);
        j["area"] = p.area;
        j["light"] = p.drivenByLight;
        j["score"] = p.score;
        portals.push(j);
    }
    root["portals"] = portals;
    Json leaks = Json::array();
    for (const auto& p : result.leaks) {
        Json j = Json::object();
        j["origin"] = vecToJson(p.origin);
        j["direction"] = vecToJson(p.direction);
        j["gap"] = p.gap;
        j["mesh"] = p.nearMesh;
        j["light"] = p.lightName;
        leaks.push(j);
    }
    root["leaks"] = leaks;
    return root;
}

Json autopsyToJson(const PixelAutopsy& autopsy) {
    Json root = Json::object();
    root["x"] = autopsy.x;
    root["y"] = autopsy.y;
    root["hit"] = autopsy.hit;
    root["mesh"] = autopsy.meshName;
    root["point"] = vecToJson(autopsy.point);
    root["normal"] = vecToJson(autopsy.normal);
    Json lights = Json::array();
    for (const auto& c : autopsy.lights) {
        Json j = Json::object();
        j["name"] = c.name;
        j["irradiance"] = c.irradiance;
        j["fraction"] = c.fraction;
        j["shadowed"] = c.shadowed;
        lights.push(j);
    }
    root["lights"] = lights;
    return root;
}

Json matchToJson(const MatchResult& match) {
    Json root = Json::object();
    root["exposureDeltaStops"] = match.exposureDeltaStops;
    Json scales = Json::array();
    for (const auto& s : match.scales) {
        Json j = Json::object();
        j["name"] = s.name;
        j["role"] = s.role;
        j["currentEnergy"] = s.currentEnergy;
        j["heroEnergy"] = s.heroEnergy;
        j["intensityScale"] = s.intensityScale;
        scales.push(j);
    }
    root["scales"] = scales;
    return root;
}

AnalysisResult analysisFromJson(const Json& root) {
    AnalysisResult r;
    if (root.has("frame")) r.frame = static_cast<int>(root.at("frame").asNumber());
    if (root.has("samples")) r.samples = static_cast<int>(root.at("samples").asNumber());
    if (root.has("hits")) r.hits = static_cast<int>(root.at("hits").asNumber());
    if (root.has("totalEnergy")) r.totalEnergy = root.at("totalEnergy").asNumber();
    if (root.has("lights")) {
        for (const auto& j : root.at("lights").asArray()) {
            LightStats l;
            l.name = j.has("name") ? j.at("name").asString() : "";
            l.type = j.has("type") ? lightTypeFromName(j.at("type").asString()) : LightType::Point;
            if (j.has("energy")) l.energy = j.at("energy").asNumber();
            if (j.has("maxSample")) l.maxSample = j.at("maxSample").asNumber();
            if (j.has("litSamples")) l.litSamples = static_cast<int>(j.at("litSamples").asNumber());
            if (j.has("pixelFraction")) l.pixelFraction = j.at("pixelFraction").asNumber();
            if (j.has("noiseScore")) l.noiseScore = j.at("noiseScore").asNumber();
            if (j.has("dead")) l.dead = j.at("dead").asBool();
            if (j.has("noisy")) l.noisy = j.at("noisy").asBool();
            if (j.has("outsideFrustum")) l.outsideFrustum = j.at("outsideFrustum").asBool();
            if (j.has("linkingEmpty")) l.linkingEmpty = j.at("linkingEmpty").asBool();
            if (j.has("role")) l.role = j.at("role").asString();
            r.lights.push_back(l);
        }
    }
    return r;
}

}  // namespace lightsurgeon
