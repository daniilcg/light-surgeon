#include "lightSurgeonHud.hpp"

#include <maya/MColor.h>
#include <maya/MFnData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
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
    : MHWRender::MPxDrawOverride(obj, nullptr, false) {}

MHWRender::DrawAPI LightSurgeonHudOverride::supportedDrawAPIs() const {
    return MHWRender::kAllDevices;
}

MUserData* LightSurgeonHudOverride::prepareForDraw(const MDagPath& objPath, const MDagPath&,
                                                   const MHWRender::MFrameContext&, MUserData* oldData) {
    LightSurgeonHudData* data = static_cast<LightSurgeonHudData*>(oldData);
    if (!data) data = new LightSurgeonHudData();
    MFnDependencyNode node(objPath.node());
    data->text = node.findPlug(LightSurgeonHud::aReport, true).asString();
    data->show = node.findPlug(LightSurgeonHud::aShow, true).asBool();
    return data;
}

void LightSurgeonHudOverride::addUIDrawables(const MDagPath&, MHWRender::MUIDrawManager& drawManager,
                                             const MHWRender::MFrameContext&, const MUserData* data) {
    const auto* hud = static_cast<const LightSurgeonHudData*>(data);
    if (!hud || !hud->show || hud->text.length() == 0) return;
    drawManager.beginDrawable();
    drawManager.setColor(MColor(0.15f, 0.9f, 0.55f, 1.0f));
    drawManager.setFontSize(MHWRender::MUIDrawManager::kSmallFontSize);
    MPoint pos(40.0, 80.0, 0.0);
    MStringArray lines;
    hud->text.split(MString("\n"), lines);
    for (unsigned i = 0; i < lines.length() && i < 40; ++i) {
        drawManager.text2d(pos, lines[i], MHWRender::MUIDrawManager::kLeft);
        pos.y += 16.0;
    }
    drawManager.endDrawable();
}
