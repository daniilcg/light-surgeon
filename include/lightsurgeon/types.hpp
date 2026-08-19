#pragma once

#include "lightsurgeon/vec3.hpp"

#include <string>
#include <vector>

namespace lightsurgeon {

enum class LightType { Directional, Point, Spot, Area, Dome };

inline const char* lightTypeName(LightType t) {
    switch (t) {
        case LightType::Directional:
            return "directional";
        case LightType::Point:
            return "point";
        case LightType::Spot:
            return "spot";
        case LightType::Area:
            return "area";
        case LightType::Dome:
            return "dome";
    }
    return "point";
}

inline LightType lightTypeFromName(const std::string& n) {
    if (n == "directional") return LightType::Directional;
    if (n == "spot") return LightType::Spot;
    if (n == "area") return LightType::Area;
    if (n == "dome") return LightType::Dome;
    return LightType::Point;
}

struct CameraDesc {
    std::string name = "persp";
    Vec3 position{0.0, 5.0, 18.0};
    Vec3 aim{0.0, 0.0, 0.0};
    Vec3 up{0.0, 1.0, 0.0};
    double fovYDegrees = 35.0;
    double nearClip = 0.1;
    double farClip = 10000.0;
    double aspect = 16.0 / 9.0;
    int width = 1920;
    int height = 1080;
};

struct LightDesc {
    std::string name;
    LightType type = LightType::Point;
    Vec3 position{0.0, 10.0, 0.0};
    Vec3 direction{0.0, -1.0, 0.0};
    Vec3 color{1.0, 1.0, 1.0};
    double intensity = 1.0;
    double exposure = 0.0;
    double radius = 0.25;
    double coneAngleDegrees = 40.0;
    double penumbraDegrees = 0.0;
    double areaWidth = 1.0;
    double areaHeight = 1.0;
    bool enabled = true;
    std::vector<std::string> includeObjects;
    std::vector<std::string> excludeObjects;
};

struct MeshDesc {
    std::string name;
    std::vector<Vec3> vertices;
    std::vector<int> triangles;  // triplets
};

struct SceneDesc {
    int frame = 1001;
    CameraDesc camera;
    std::vector<LightDesc> lights;
    std::vector<MeshDesc> meshes;
};

struct AnalyzeSettings {
    int sampleWidth = 96;
    int sampleHeight = 54;
    int areaSamples = 5;
    double shadowBias = 0.001;
    double deadEnergyEpsilon = 1e-8;
    double noisyRadius = 0.08;
    double noisyEnergyDensity = 80.0;
    double noisyPixelFraction = 0.03;
    double leakGap = 0.08;
    int leakSamples = 48;
    int maxTriangles = 400000;
};

struct LightStats {
    std::string name;
    LightType type = LightType::Point;
    double energy = 0.0;
    double maxSample = 0.0;
    int litSamples = 0;
    double pixelFraction = 0.0;
    double noiseScore = 0.0;
    bool dead = false;
    bool noisy = false;
    bool outsideFrustum = false;
    bool linkingEmpty = false;
    std::string role = "utility";
};

struct Contribution {
    std::string name;
    double irradiance = 0.0;
    double fraction = 0.0;
    bool shadowed = false;
};

struct PixelAutopsy {
    int x = 0;
    int y = 0;
    bool hit = false;
    std::string meshName;
    Vec3 point;
    Vec3 normal;
    std::vector<Contribution> lights;
};

struct PortalCandidate {
    std::string meshName;
    Vec3 center;
    Vec3 normal;
    double area = 0.0;
    std::string drivenByLight;
    double score = 0.0;
};

struct LeakHit {
    Vec3 origin;
    Vec3 direction;
    double gap = 0.0;
    std::string nearMesh;
    std::string lightName;
};

struct MatchScale {
    std::string name;
    std::string role;
    double currentEnergy = 0.0;
    double heroEnergy = 0.0;
    double intensityScale = 1.0;
};

struct MatchResult {
    std::vector<MatchScale> scales;
    double exposureDeltaStops = 0.0;
};

struct AnalysisResult {
    int frame = 1001;
    int samples = 0;
    int hits = 0;
    std::vector<LightStats> lights;
    std::vector<PortalCandidate> portals;
    std::vector<LeakHit> leaks;
    double totalEnergy = 0.0;
};

}  // namespace lightsurgeon
