#include "lightsurgeon/bvh.hpp"
#include "lightsurgeon/engine.hpp"
#include "lightsurgeon/scene_io.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace lightsurgeon;

static int gFails = 0;
static int gPass = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        ++gPass;
        std::cout << "  PASS  " << name << "\n";
    } else {
        ++gFails;
        std::cout << "  FAIL  " << name << "\n";
    }
}

static MeshDesc makePlane(const std::string& name, Vec3 origin, Vec3 u, Vec3 v) {
    MeshDesc m;
    m.name = name;
    m.vertices = {origin, origin + u, origin + u + v, origin + v};
    m.triangles = {0, 1, 2, 0, 2, 3};
    return m;
}

static SceneDesc studioScene() {
    SceneDesc s;
    s.frame = 1001;
    s.camera.position = {0.0, 3.0, 10.0};
    s.camera.aim = {0.0, 0.0, 0.0};
    s.camera.up = {0.0, 1.0, 0.0};
    s.camera.fovYDegrees = 40.0;
    s.camera.aspect = 16.0 / 9.0;
    s.camera.width = 1920;
    s.camera.height = 1080;
    s.meshes.push_back(makePlane("floor", {-8.0, 0.0, -8.0}, {16.0, 0.0, 0.0}, {0.0, 0.0, 16.0}));

    LightDesc key;
    key.name = "key";
    key.type = LightType::Directional;
    key.direction = {-0.35, -1.0, -0.4};
    key.intensity = 4.0;
    key.color = {1.0, 0.96, 0.9};
    s.lights.push_back(key);

    LightDesc fill;
    fill.name = "fill";
    fill.type = LightType::Point;
    fill.position = {-3.5, 2.5, 4.0};
    fill.intensity = 40.0;
    fill.radius = 0.4;
    s.lights.push_back(fill);

    LightDesc rim;
    rim.name = "rim";
    rim.type = LightType::Directional;
    rim.direction = {0.1, -0.2, -1.0};
    rim.intensity = 1.5;
    s.lights.push_back(rim);

    LightDesc dead;
    dead.name = "dead_below";
    dead.type = LightType::Point;
    dead.position = {0.0, -3.0, 0.0};
    dead.intensity = 80.0;
    dead.radius = 0.2;
    s.lights.push_back(dead);

    LightDesc fire;
    fire.name = "firefly";
    fire.type = LightType::Point;
    fire.position = {7.4, 0.05, -7.4};
    fire.intensity = 12000.0;
    fire.radius = 0.01;
    s.lights.push_back(fire);

    LightDesc off;
    off.name = "disabled";
    off.type = LightType::Point;
    off.position = {0.0, 5.0, 0.0};
    off.intensity = 50.0;
    off.enabled = false;
    s.lights.push_back(off);

    return s;
}

static SceneDesc interiorScene() {
    SceneDesc s;
    s.camera.position = {0.0, 1.4, 0.0};
    s.camera.aim = {0.0, 1.4, 4.0};
    s.camera.fovYDegrees = 60.0;
    s.meshes.push_back(makePlane("floor", {-3.0, 0.0, -3.0}, {6.0, 0.0, 0.0}, {0.0, 0.0, 6.0}));
    s.meshes.push_back(makePlane("window", {-1.2, 0.4, 3.0}, {0.0, 2.2, 0.0}, {2.4, 0.0, 0.0}));
    LightDesc sun;
    sun.name = "sun";
    sun.type = LightType::Directional;
    sun.direction = {0.0, -0.15, -1.0};
    sun.intensity = 8.0;
    s.lights.push_back(sun);
    return s;
}

static std::string fixtureFile(const std::string& name) {
    const char* dirs[] = {
#ifdef LS_FIXTURE_DIR
        LS_FIXTURE_DIR,
#endif
        "tests/fixtures",
        "../tests/fixtures",
        "../../tests/fixtures",
        "fixtures"};
    for (const char* dir : dirs) {
        std::string path = std::string(dir) + "/" + name;
        std::ifstream in(path.c_str());
        if (in) return path;
    }
    return std::string("tests/fixtures/") + name;
}

static const LightStats* findLight(const AnalysisResult& r, const std::string& name) {
    for (const auto& l : r.lights) {
        if (l.name == name) return &l;
    }
    return nullptr;
}

int main() {
    std::cout << "Light Surgeon tests\n";

    const Json parsed = Json::parse("{\"a\":[1,true,null,\"x\"],\"b\":-2.5e3}");
    check(parsed.at("a").asArray().size() == 4, "json array size");
    check(parsed.at("a").asArray()[1].asBool(), "json bool");
    check(parsed.at("a").asArray()[2].isNull(), "json null");
    check(parsed.at("a").asArray()[3].asString() == "x", "json string");
    check(std::abs(parsed.at("b").asNumber() + 2500.0) < 1e-9, "json scientific number");
    const std::string dumped = parsed.dump();
    check(Json::parse(dumped).at("b").asNumber() == parsed.at("b").asNumber(), "json roundtrip");

    SceneDesc empty;
    const AnalysisResult emptyResult = analyzeScene(empty);
    check(emptyResult.lights.empty(), "empty scene has no lights");
    check(emptyResult.hits == 0, "empty scene has no hits");

    const SceneDesc studio = studioScene();
    const Json sceneJson = sceneToJson(studio);
    const SceneDesc reloaded = sceneFromJson(sceneJson);
    check(reloaded.lights.size() == studio.lights.size(), "scene json light count");
    check(reloaded.meshes[0].triangles.size() == 6, "scene json triangle indices");

    AnalyzeSettings settings;
    settings.sampleWidth = 64;
    settings.sampleHeight = 36;
    const AnalysisResult analysis = analyzeScene(studio, settings);
    check(analysis.hits > 100, "studio camera hits floor");
    const LightStats* key = findLight(analysis, "key");
    const LightStats* dead = findLight(analysis, "dead_below");
    const LightStats* fill = findLight(analysis, "fill");
    const LightStats* fire = findLight(analysis, "firefly");
    const LightStats* off = findLight(analysis, "disabled");
    check(key && key->energy > 0.0 && !key->dead, "key light contributes");
    check(fill && fill->energy > 0.0, "fill light contributes");
    check(dead && dead->dead, "light under floor is dead");
    check(off && off->energy == 0.0, "disabled light has zero energy");
    check(fire && fire->noisy, "tiny hot light is noisy");
    check(analysis.totalEnergy > key->energy * 0.5, "total energy consistent");
    check(!formatReport(analysis).empty(), "text report not empty");

    const PixelAutopsy pix = autopsyPixel(studio, 960, 540, settings);
    check(pix.hit, "center pixel hits floor");
    check(pix.hit && pix.lights.size() >= 2 && pix.lights[0].irradiance >= pix.lights[1].irradiance,
          "pixel autopsy sorted by irradiance");
    double fracSum = 0.0;
    for (const auto& c : pix.lights) fracSum += c.fraction;
    check(std::abs(fracSum - 1.0) < 1e-6 || !pix.hit, "autopsy fractions sum to 1");

    SceneDesc roofed = studio;
    LightDesc overhead;
    overhead.name = "overhead";
    overhead.type = LightType::Point;
    overhead.position = {0.0, 8.0, 0.0};
    overhead.intensity = 400.0;
    overhead.radius = 0.2;
    roofed.lights.push_back(overhead);
    const AnalysisResult openRoof = analyzeScene(roofed, settings);
    roofed.meshes.push_back(makePlane("ceiling", {-9.0, 4.0, -9.0}, {18.0, 0.0, 0.0}, {0.0, 0.0, 18.0}));
    const AnalysisResult closedRoof = analyzeScene(roofed, settings);
    const LightStats* ohOpen = findLight(openRoof, "overhead");
    const LightStats* ohClosed = findLight(closedRoof, "overhead");
    check(ohOpen && ohOpen->energy > 0.0, "unoccluded overhead light contributes");
    check(ohClosed && ohClosed->energy < ohOpen->energy * 0.05, "ceiling occludes overhead light");

    SceneDesc linked = studio;
    linked.lights[0].excludeObjects = {"floor"};
    const AnalysisResult linkedRes = analyzeScene(linked, settings);
    const LightStats* keyLinked = findLight(linkedRes, "key");
    check(keyLinked && keyLinked->dead, "excluded floor makes key dead");

    AnalysisResult hero = analysis;
    for (auto& l : hero.lights) {
        if (l.role == "key") l.energy *= 2.0;
        hero.totalEnergy += l.energy;
    }
    hero.totalEnergy = 0.0;
    for (const auto& l : hero.lights) hero.totalEnergy += l.energy;
    const MatchResult match = matchHero(analysis, hero);
    check(std::abs(match.exposureDeltaStops) > 0.1, "hero match exposure delta");
    bool keyScale = false;
    for (const auto& s : match.scales) {
        if (s.role == "key" && s.intensityScale > 1.4 && s.intensityScale < 2.6) keyScale = true;
    }
    check(keyScale, "hero match scales key ~2x");

    const AnalysisResult interior = analyzeScene(interiorScene(), settings);
    check(!interior.portals.empty(), "window produces portal candidate");
    check(interior.portals[0].drivenByLight == "sun", "portal driven by sun");

    std::vector<Triangle> tris;
    Triangle t;
    t.v0 = {-1, 0, -1};
    t.v1 = {1, 0, -1};
    t.v2 = {0, 0, 1};
    t.meshIndex = 0;
    tris.push_back(t);
    Bvh bvh;
    bvh.build(tris);
    Ray ray;
    ray.origin = {0.0, 2.0, 0.0};
    ray.direction = {0.0, -1.0, 0.0};
    const Hit hit = bvh.closestHit(ray);
    check(hit.hit, "bvh hits triangle");
    check(std::abs(hit.t - 2.0) < 1e-6, "bvh hit distance");
    Ray miss = ray;
    miss.origin = {5.0, 2.0, 5.0};
    check(!bvh.closestHit(miss).hit, "bvh miss");
    Ray occ;
    occ.origin = {0.0, 2.0, 0.0};
    occ.direction = {0.0, -1.0, 0.0};
    occ.tMax = 3.0;
    check(bvh.occluded(occ), "bvh occlusion");

    const SceneDesc fileScene = loadSceneFile(fixtureFile("studio.json"));
    const AnalysisResult fileAnalysis = analyzeScene(fileScene, settings);
    check(fileAnalysis.hits > 0, "fixture studio.json analyzes");
    const auto fileAutopsy = autopsyPixel(fileScene, 960, 540, settings);
    check(fileAutopsy.hit, "fixture pixel hit");

    std::cout << gPass << " passed, " << gFails << " failed\n";
    return gFails == 0 ? 0 : 1;
}
