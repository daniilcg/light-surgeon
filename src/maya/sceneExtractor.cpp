#include "sceneExtractor.hpp"

#include <algorithm>
#include <cmath>

#include <maya/M3dView.h>
#include <maya/MAnimControl.h>
#include <maya/MDagPath.h>
#include <maya/MFloatVector.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDirectionalLight.h>
#include <maya/MFnLight.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSpotLight.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDag.h>
#include <maya/MLightLinks.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>

namespace lightsurgeon {
namespace {

Vec3 toVec(const MPoint& p) { return {p.x, p.y, p.z}; }
Vec3 toVec(const MVector& p) { return {p.x, p.y, p.z}; }
Vec3 toVec(const MFloatVector& p) { return {p.x, p.y, p.z}; }

double plugDouble(const MFnDependencyNode& node, const char* name, double fallback) {
    const MPlug plug = node.findPlug(name, true);
    if (plug.isNull()) return fallback;
    return plug.asDouble();
}

bool plugExists(const MFnDependencyNode& node, const char* name) {
    return !node.findPlug(name, true).isNull();
}

Vec3 plugColor(const MFnDependencyNode& node, const char* name, const Vec3& fallback) {
    MPlug plug = node.findPlug(name, true);
    if (plug.isNull() || plug.numChildren() < 3) return fallback;
    return {plug.child(0).asDouble(), plug.child(1).asDouble(), plug.child(2).asDouble()};
}

LightDesc extractLight(const MDagPath& path) {
    LightDesc light;
    MFnDagNode dag(path);
    light.name = dag.partialPathName().asChar();
    const MMatrix wm = path.inclusiveMatrix();
    const MPoint origin = MPoint(0, 0, 0) * wm;
    const MVector down = MVector(0, 0, -1) * wm - MVector(0, 0, 0) * wm;
    light.position = toVec(origin);
    light.direction = normalize(toVec(down));
    if (length(light.direction) < 1e-12) light.direction = {0.0, -1.0, 0.0};
    light.enabled = true;
    light.intensity = 1.0;
    light.color = {1.0, 1.0, 1.0};

    if (path.hasFn(MFn::kLight)) {
        MFnLight fnLight(path);
        light.color = toVec(fnLight.color());
        light.intensity = fnLight.intensity();
    }

    const MFnDependencyNode dep(path.node());
    light.exposure = plugDouble(dep, "aiExposure", plugDouble(dep, "exposure", 0.0));
    light.radius = plugDouble(dep, "aiRadius", plugDouble(dep, "lightRadius", 0.25));
    if (path.hasFn(MFn::kDirectionalLight)) {
        light.type = LightType::Directional;
        MFnDirectionalLight dir(path);
        light.intensity = dir.intensity();
    } else if (path.hasFn(MFn::kSpotLight)) {
        light.type = LightType::Spot;
        MFnSpotLight spot(path);
        light.coneAngleDegrees = spot.coneAngle() * 180.0 / 3.14159265358979323846;
        light.penumbraDegrees = spot.penumbraAngle() * 180.0 / 3.14159265358979323846;
    } else if (path.hasFn(MFn::kAreaLight)) {
        light.type = LightType::Area;
        const MVector sx = MVector(1, 0, 0) * wm - MVector(0, 0, 0) * wm;
        const MVector sy = MVector(0, 1, 0) * wm - MVector(0, 0, 0) * wm;
        light.areaWidth = std::max(0.01, 2.0 * sx.length());
        light.areaHeight = std::max(0.01, 2.0 * sy.length());
    } else {
        light.type = LightType::Point;
    }

    const MString typeName = dag.typeName();
    if (typeName == "aiSkyDomeLight") {
        light.type = LightType::Dome;
        light.color = plugColor(dep, "color", light.color);
        light.intensity = plugDouble(dep, "intensity", light.intensity);
    } else if (typeName == "aiAreaLight") {
        light.type = LightType::Area;
        light.areaWidth = plugDouble(dep, "aiScaleX", light.areaWidth);
        light.areaHeight = plugDouble(dep, "aiScaleY", light.areaHeight);
        light.intensity = plugDouble(dep, "intensity", light.intensity);
    } else if (typeName == "aiPhotometricLight" || typeName == "aiMeshLight") {
        light.type = LightType::Area;
    }

    if (plugExists(dep, "visibility")) {
        light.enabled = dep.findPlug("visibility", true).asBool() && light.intensity > 0.0;
    }
    if (path.hasFn(MFn::kLight)) {
        MFnLight fnLight(path);
        if (!fnLight.illuminatesByDefault()) {
            light.includeObjects.push_back("__none__");
        }
    }
    return light;
}

bool dagVisible(MDagPath path) {
    while (path.length() > 0) {
        MFnDagNode dag(path);
        MPlug vis = dag.findPlug("visibility", true);
        MPlug lod = dag.findPlug("lodVisibility", true);
        if (!vis.isNull() && !vis.asBool()) return false;
        if (!lod.isNull() && !lod.asBool()) return false;
        if (path.pop() != MS::kSuccess) break;
    }
    return true;
}

void applyLightLinking(std::vector<LightDesc>* lights) {
    if (!lights || lights->empty()) return;
    MLightLinks linker;
    if (linker.parseLinks(MObject::kNullObj) != MS::kSuccess) return;
    for (LightDesc& light : *lights) {
        MSelectionList list;
        if (list.add(light.name.c_str()) != MS::kSuccess) continue;
        MDagPath path;
        if (list.getDagPath(0, path) != MS::kSuccess) continue;
        if (path.hasFn(MFn::kTransform)) path.extendToShape();
        MSelectionList linked;
        if (linker.getLinkedObjects(path, false, linked) != MS::kSuccess) continue;
        if (path.hasFn(MFn::kLight) && MFnLight(path).illuminatesByDefault()) {
            MSelectionList ignored;
            if (linker.getLinkedObjects(path, true, ignored) == MS::kSuccess) {
                light.excludeObjects.clear();
                for (unsigned i = 0; i < ignored.length(); ++i) {
                    MDagPath obj;
                    if (ignored.getDagPath(i, obj) == MS::kSuccess) {
                        light.excludeObjects.emplace_back(MFnDagNode(obj).partialPathName().asChar());
                    }
                }
            }
            light.includeObjects.clear();
        } else {
            light.includeObjects.clear();
            for (unsigned i = 0; i < linked.length(); ++i) {
                MDagPath obj;
                if (linked.getDagPath(i, obj) == MS::kSuccess) {
                    light.includeObjects.emplace_back(MFnDagNode(obj).partialPathName().asChar());
                }
            }
        }
    }
}

MStatus fillCamera(SceneDesc* scene, MString* error) {
    MDagPath camPath;
    MString renderCam;
    MGlobal::executeCommand("if (`objExists renderView`) renderWindowEditor -q -currentCamera renderView;", renderCam);
    bool got = false;
    if (renderCam.length() > 0) {
        MSelectionList sl;
        if (sl.add(renderCam) == MS::kSuccess && sl.getDagPath(0, camPath) == MS::kSuccess) {
            if (camPath.hasFn(MFn::kTransform)) camPath.extendToShape();
            got = camPath.hasFn(MFn::kCamera);
        }
    }
    if (!got) {
        M3dView view = M3dView::active3dView();
        if (view.getCamera(camPath) != MS::kSuccess) {
            if (error) *error = "No render or viewport camera";
            return MS::kFailure;
        }
    }
    MFnCamera cam(camPath);
    scene->camera.name = MFnDagNode(camPath).partialPathName().asChar();
    const MMatrix wm = camPath.inclusiveMatrix();
    const MPoint eye = MPoint(0, 0, 0) * wm;
    const MVector dir = MVector(0, 0, -1) * wm - MVector(0, 0, 0) * wm;
    const MVector up = MVector(0, 1, 0) * wm - MVector(0, 0, 0) * wm;
    scene->camera.position = toVec(eye);
    scene->camera.aim = toVec(eye + dir);
    scene->camera.up = normalize(toVec(up));
    scene->camera.fovYDegrees = cam.verticalFieldOfView() * 180.0 / 3.14159265358979323846;
    scene->camera.nearClip = cam.nearClippingPlane();
    scene->camera.farClip = cam.farClippingPlane();
    int resW = 1920;
    int resH = 1080;
    MGlobal::executeCommand("getAttr defaultResolution.width", resW);
    MGlobal::executeCommand("getAttr defaultResolution.height", resH);
    scene->camera.width = resW;
    scene->camera.height = resH;
    double aspect = 0.0;
    MGlobal::executeCommand("getAttr defaultResolution.deviceAspectRatio", aspect);
    scene->camera.aspect = aspect > 0.0 ? aspect : static_cast<double>(resW) / std::max(1, resH);
    return MS::kSuccess;
}

MeshDesc extractMesh(const MDagPath& path, int* remainingTris) {
    MeshDesc mesh;
    MFnDagNode dag(path);
    mesh.name = dag.partialPathName().asChar();
    MFnMesh fn(path);
    MPointArray points;
    fn.getPoints(points, MSpace::kWorld);
    mesh.vertices.reserve(points.length());
    for (unsigned i = 0; i < points.length(); ++i) {
        mesh.vertices.push_back(toVec(points[i]));
    }
    MIntArray counts;
    MIntArray indices;
    fn.getTriangles(counts, indices);
    const int maxTake = *remainingTris * 3;
    const int take = std::min(static_cast<int>(indices.length()), maxTake);
    mesh.triangles.reserve(static_cast<std::size_t>(take));
    for (int i = 0; i < take; ++i) {
        mesh.triangles.push_back(indices[static_cast<unsigned>(i)]);
    }
    *remainingTris -= take / 3;
    return mesh;
}

}  // namespace

MStatus extractMayaScene(SceneDesc* scene, MString* error) {
    if (!scene) return MS::kFailure;
    *scene = SceneDesc();
    scene->frame = static_cast<int>(MAnimControl::currentTime().value());
    const MStatus camSt = fillCamera(scene, error);
    if (camSt != MS::kSuccess) return camSt;

    int remaining = 400000;
    MItDag dagIt(MItDag::kDepthFirst, MFn::kInvalid);
    for (; !dagIt.isDone(); dagIt.next()) {
        MDagPath path;
        dagIt.getPath(path);
        if (!dagVisible(path)) continue;
        if (path.hasFn(MFn::kLight)) {
            scene->lights.push_back(extractLight(path));
        } else if (path.hasFn(MFn::kMesh) && remaining > 0) {
            MFnDagNode dag(path);
            if (dag.isIntermediateObject()) continue;
            scene->meshes.push_back(extractMesh(path, &remaining));
        } else {
            MFnDagNode dag(path);
            const MString tn = dag.typeName();
            if (tn == "aiSkyDomeLight" || tn == "aiAreaLight" || tn == "aiPhotometricLight" || tn == "aiMeshLight") {
                scene->lights.push_back(extractLight(path));
            }
        }
    }
    applyLightLinking(&scene->lights);
    return MS::kSuccess;
}

}  // namespace lightsurgeon
