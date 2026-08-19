#pragma once

#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MStringArray.h>
#include <maya/MDoubleArray.h>

class LightSurgeonCmd : public MPxCommand {
public:
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
    MStatus redoIt() override;
    MStatus undoIt() override;
    bool isUndoable() const override;

private:
    enum class Action { None, Mute, Restore, Solo, Scale };

    MStatus analyzeAndPrint(bool writeHud, bool asJson, int frameStart, int frameEnd, int frameStep);
    MStatus runPixel(int x, int y);
    MStatus writeReport(const MString& path);
    MStatus exportScene(const MString& path);
    MStatus matchHero(const MString& path, bool apply);
    MStatus applyMuting(bool dead, bool noisy);
    MStatus applySolo(const MString& name);
    MStatus restoreAll();
    MStatus applyScales();
    MStatus ensureHud(const MString& report);
    static MStatus setLightIntensity(const MString& name, double value);
    static MStatus getLightIntensity(const MString& name, double* value);

    Action action_ = Action::None;
    MStringArray names_;
    MDoubleArray oldIntensity_;
    MDoubleArray newIntensity_;
    MString resultText_;
};
