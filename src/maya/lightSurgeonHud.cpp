#include "lightSurgeonHud.hpp"

#include <maya/MBoundingBox.h>
#include <maya/MColor.h>
#include <maya/MFnCamera.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFrameContext.h>
#include <maya/MHWGeometry.h>
#include <maya/MUIDrawManager.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

const MTypeId LightSurgeonHud::id(0x0013C301);
const MString LightSurgeonHud::drawDbClassification("drawdb/geometry/lightSurgeonHud");
const MString LightSurgeonHud::drawRegistrantId("LightSurgeonHudOverride");
MObject LightSurgeonHud::aReport;
MObject LightSurgeonHud::aShow;

void* LightSurgeonHud::creator() { return new LightSurgeonHud(); }

MStatus LightSurgeonHud::initialize() {
    MFnTypedAttribute tAttr;
    MFnNumericAttribute nAttr;
    aReport = tAttr.create("report", "rep", MFnData::kString);
    tAttr.setStorable(true);
    tAttr.setKeyable(false);
    aShow = nAttr.create("showHud", "shd", MFnNumericData::kBoolean, 1);
    nAttr.setStorable(true);
    nAttr.setKeyable(true);
    addAttribute(aReport);
    addAttribute(aShow);
    return MS::kSuccess;
}

MHWRender::MPxDrawOverride* LightSurgeonHudOverride::creator(const MObject& obj) {
    return new LightSurgeonHudOverride(obj);
}

LightSurgeonHudOverride::LightSurgeonHudOverride(const MObject& obj)
    : MHWRender::MPxDrawOverride(obj, nullptr, true) {}

MHWRender::DrawAPI LightSurgeonHudOverride::supportedDrawAPIs() const {
    return MHWRender::kAllDevices;
}

bool LightSurgeonHudOverride::isBounded(const MDagPath&, const MDagPath&) const { return true; }

MBoundingBox LightSurgeonHudOverride::boundingBox(const MDagPath&, const MDagPath&) const {
    return MBoundingBox(MPoint(-1.0, -1.0, -1.0), MPoint(1.0, 1.0, 1.0));
}

bool LightSurgeonHudOverride::excludedFromPostEffects() const { return true; }

MUserData* LightSurgeonHudOverride::prepareForDraw(const MDagPath& objPath, const MDagPath& cameraPath,
                                                   const MHWRender::MFrameContext& frameContext, MUserData* oldData) {
    LightSurgeonHudData* data = static_cast<LightSurgeonHudData*>(oldData);
    if (!data) data = new LightSurgeonHudData();
    MFnDependencyNode node(objPath.node());
    data->text = node.findPlug(LightSurgeonHud::aReport, true).asString();
    data->show = node.findPlug(LightSurgeonHud::aShow, true).asBool();
    data->perspOnly = true;
    if (cameraPath.isValid() && cameraPath.hasFn(MFn::kCamera)) {
        MFnCamera cam(cameraPath);
        data->perspOnly = !cam.isOrtho();
    }
    int ox = 0;
    int oy = 0;
    int vw = 1920;
    int vh = 1080;
    frameContext.getViewportDimensions(ox, oy, vw, vh);
    data->viewHeight = vh > 0 ? vh : 1080;
    return data;
}

void LightSurgeonHudOverride::addUIDrawables(const MDagPath&, MHWRender::MUIDrawManager& drawManager,
                                             const MHWRender::MFrameContext&, const MUserData* data) {
    const auto* hud = static_cast<const LightSurgeonHudData*>(data);
    if (!hud || !hud->show || !hud->perspOnly || hud->text.length() == 0) return;
    MStringArray lines;
    hud->text.split('\n', lines);
    const unsigned count = lines.length() < 10 ? lines.length() : 10;
    drawManager.beginDrawable(MHWRender::MUIDrawManager::kNonSelectable);
    drawManager.setFontSize(MHWRender::MUIDrawManager::kSmallFontSize);
    drawManager.setColor(MColor(0.35f, 0.95f, 0.55f, 1.0f));
    MPoint pos(18.0, 22.0, 0.0);
    for (unsigned i = 0; i < count; ++i) {
        drawManager.text2d(pos, lines[i], MHWRender::MUIDrawManager::kLeft);
        pos.y += 16.0;
    }
    drawManager.endDrawable();
}
