#pragma once

#include <maya/MBoundingBox.h>
#include <maya/MDrawRegistry.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MTypeId.h>
#include <maya/MUserData.h>

class LightSurgeonHud : public MPxLocatorNode {
public:
    static void* creator();
    static MStatus initialize();
    static const MTypeId id;
    static const MString drawDbClassification;
    static const MString drawRegistrantId;
    static MObject aReport;
    static MObject aShow;
};

class LightSurgeonHudData : public MUserData {
public:
    LightSurgeonHudData() : MUserData() {}
    MString text;
    bool show = true;
    bool perspOnly = false;
    int viewHeight = 720;
};

class LightSurgeonHudOverride : public MHWRender::MPxDrawOverride {
public:
    static MHWRender::MPxDrawOverride* creator(const MObject& obj);
    explicit LightSurgeonHudOverride(const MObject& obj);
    MHWRender::DrawAPI supportedDrawAPIs() const override;
    bool hasUIDrawables() const override { return true; }
    bool isBounded(const MDagPath& objPath, const MDagPath& cameraPath) const override;
    MBoundingBox boundingBox(const MDagPath& objPath, const MDagPath& cameraPath) const override;
    bool excludedFromPostEffects() const override;
    MUserData* prepareForDraw(const MDagPath& objPath, const MDagPath& cameraPath,
                              const MHWRender::MFrameContext& frameContext, MUserData* oldData) override;
    void addUIDrawables(const MDagPath& objPath, MHWRender::MUIDrawManager& drawManager,
                        const MHWRender::MFrameContext& frameContext, const MUserData* data) override;
};
