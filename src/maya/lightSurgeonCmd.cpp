#include "lightSurgeonCmd.hpp"
#include "sceneExtractor.hpp"

#include "lightsurgeon/engine.hpp"
#include "lightsurgeon/scene_io.hpp"

#include <maya/MAnimControl.h>
#include <maya/MArgDatabase.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MSyntax.h>
#include <maya/MTime.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

static const char* kAnalyzeFlag = "-an";
static const char* kAnalyzeLong = "-analyze";
static const char* kPixelFlag = "-px";
static const char* kPixelLong = "-pixel";
static const char* kReportFlag = "-rp";
static const char* kReportLong = "-report";
static const char* kMuteDeadFlag = "-md";
static const char* kMuteDeadLong = "-muteDead";
static const char* kMuteNoisyFlag = "-mn";
static const char* kMuteNoisyLong = "-muteNoisy";
static const char* kSoloFlag = "-so";
static const char* kSoloLong = "-solo";
static const char* kRestoreFlag = "-rs";
static const char* kRestoreLong = "-restore";
static const char* kMatchFlag = "-mh";
static const char* kMatchLong = "-matchHero";
static const char* kApplyFlag = "-ap";
static const char* kApplyLong = "-apply";
static const char* kExportFlag = "-ex";
static const char* kExportLong = "-exportScene";
static const char* kHudFlag = "-hd";
static const char* kHudLong = "-hud";
static const char* kPortalsFlag = "-pt";
static const char* kPortalsLong = "-portals";
static const char* kJsonFlag = "-js";
static const char* kJsonLong = "-json";
static const char* kPixelCenterFlag = "-pc";
static const char* kPixelCenterLong = "-pixelCenter";
static const char* kStartFlag = "-fs";
static const char* kStartLong = "-frameStart";
static const char* kEndFlag = "-fe";
static const char* kEndLong = "-frameEnd";
static const char* kStepFlag = "-ft";
static const char* kStepLong = "-frameStep";

void* LightSurgeonCmd::creator() { return new LightSurgeonCmd(); }

MSyntax LightSurgeonCmd::newSyntax() {
    MSyntax syn;
    syn.addFlag(kAnalyzeFlag, kAnalyzeLong);
    syn.addFlag(kPixelFlag, kPixelLong, MSyntax::kLong, MSyntax::kLong);
    syn.addFlag(kReportFlag, kReportLong, MSyntax::kString);
    syn.addFlag(kMuteDeadFlag, kMuteDeadLong);
    syn.addFlag(kMuteNoisyFlag, kMuteNoisyLong);
    syn.addFlag(kSoloFlag, kSoloLong, MSyntax::kString);
    syn.addFlag(kRestoreFlag, kRestoreLong);
    syn.addFlag(kMatchFlag, kMatchLong, MSyntax::kString);
    syn.addFlag(kApplyFlag, kApplyLong);
    syn.addFlag(kExportFlag, kExportLong, MSyntax::kString);
    syn.addFlag(kHudFlag, kHudLong, MSyntax::kBoolean);
    syn.addFlag(kPortalsFlag, kPortalsLong);
    syn.addFlag(kJsonFlag, kJsonLong);
    syn.addFlag(kPixelCenterFlag, kPixelCenterLong);
    syn.addFlag(kStartFlag, kStartLong, MSyntax::kLong);
    syn.addFlag(kEndFlag, kEndLong, MSyntax::kLong);
    syn.addFlag(kStepFlag, kStepLong, MSyntax::kLong);
    return syn;
}

MStatus LightSurgeonCmd::getLightIntensity(const MString& name, double* value) {
    MSelectionList list;
    MStatus st = list.add(name);
    if (st != MS::kSuccess) return st;
    MDagPath path;
    st = list.getDagPath(0, path);
    if (st != MS::kSuccess) return st;
    MFnDependencyNode node(path.node());
    MPlug plug = node.findPlug("intensity", true, &st);
    if (st != MS::kSuccess || plug.isNull()) {
        plug = node.findPlug("aiIntensity", true, &st);
    }
    if (st != MS::kSuccess || plug.isNull()) return MS::kFailure;
    *value = plug.asDouble();
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::setLightIntensity(const MString& name, double value) {
    MSelectionList list;
    MStatus st = list.add(name);
    if (st != MS::kSuccess) return st;
    MDagPath path;
    st = list.getDagPath(0, path);
    if (st != MS::kSuccess) return st;
    MFnDependencyNode node(path.node());
    MPlug plug = node.findPlug("intensity", true, &st);
    if (st != MS::kSuccess || plug.isNull()) {
        plug = node.findPlug("aiIntensity", true, &st);
    }
    if (st != MS::kSuccess || plug.isNull()) return MS::kFailure;
    MPlug saved = node.findPlug("ls_savedIntensity", true);
    if (saved.isNull()) {
        MFnNumericAttribute nAttr;
        MObject attr = nAttr.create("ls_savedIntensity", "lssi", MFnNumericData::kDouble, plug.asDouble());
        nAttr.setStorable(true);
        nAttr.setHidden(true);
        node.addAttribute(attr);
        saved = node.findPlug("ls_savedIntensity", true);
        if (!saved.isNull()) saved.setDouble(plug.asDouble());
    }
    return plug.setDouble(value);
}

namespace {

MStatus runAnalysis(lightsurgeon::AnalysisResult* result, MString* error, int frameStart, int frameEnd, int frameStep) {
    const MTime saved = MAnimControl::currentTime();
    const int current = static_cast<int>(saved.value());
    int start = frameStart;
    int end = frameEnd;
    int step = std::max(1, frameStep);
    if (start == 0 && end == 0) {
        start = current;
        end = current;
    }
    if (end < start) end = start;
    std::vector<lightsurgeon::AnalysisResult> parts;
    for (int f = start; f <= end; f += step) {
        if (f != current) {
            MAnimControl::setCurrentTime(MTime(f, MTime::uiUnit()));
        }
        lightsurgeon::SceneDesc scene;
        const MStatus st = lightsurgeon::extractMayaScene(&scene, error);
        if (st != MS::kSuccess) {
            MAnimControl::setCurrentTime(saved);
            return st;
        }
        parts.push_back(lightsurgeon::analyzeScene(scene));
    }
    MAnimControl::setCurrentTime(saved);
    *result = parts.size() == 1 ? parts.front() : lightsurgeon::mergeAnalyses(parts);
    return MS::kSuccess;
}

}  // namespace

MStatus LightSurgeonCmd::ensureHud(const MString& report) {
    MSelectionList existing;
    if (existing.add("lightSurgeonHud1") != MS::kSuccess) {
        MGlobal::executeCommand("createNode -n lightSurgeonHud1 lightSurgeonHud");
        existing.clear();
        if (existing.add("lightSurgeonHud1") != MS::kSuccess) return MS::kFailure;
    }
    MObject nodeObj;
    existing.getDependNode(0, nodeObj);
    MFnDependencyNode node(nodeObj);
    MPlug plug = node.findPlug("report", true);
    if (!plug.isNull()) plug.setString(report);
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::analyzeAndPrint(bool writeHud, bool asJson, int frameStart, int frameEnd, int frameStep) {
    lightsurgeon::AnalysisResult result;
    MString err;
    const MStatus st = runAnalysis(&result, &err, frameStart, frameEnd, frameStep);
    if (st != MS::kSuccess) {
        if (err.length()) MGlobal::displayError(err);
        return st;
    }
    resultText_ = asJson ? lightsurgeon::analysisToJson(result).dump(2).c_str()
                         : lightsurgeon::formatReport(result).c_str();
    if (writeHud) ensureHud(lightsurgeon::formatReport(result).c_str());
    setResult(resultText_);
    MGlobal::displayInfo(resultText_);
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::runPixel(int x, int y) {
    lightsurgeon::SceneDesc scene;
    MString err;
    const MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
    if (st != MS::kSuccess) {
        if (err.length()) MGlobal::displayError(err);
        return st;
    }
    const auto autopsy = lightsurgeon::autopsyPixel(scene, x, y);
    resultText_ = lightsurgeon::autopsyToJson(autopsy).dump(2).c_str();
    setResult(resultText_);
    MGlobal::displayInfo(resultText_);
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::writeReport(const MString& path) {
    lightsurgeon::AnalysisResult result;
    MString err;
    const MStatus st = runAnalysis(&result, &err, 0, 0, 1);
    if (st != MS::kSuccess) {
        if (err.length()) MGlobal::displayError(err);
        return st;
    }
    std::ofstream out(path.asChar(), std::ios::binary);
    if (!out) {
        MGlobal::displayError("Cannot write report");
        return MS::kFailure;
    }
    out << lightsurgeon::analysisToJson(result).dump(2);
    setResult(path);
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::exportScene(const MString& path) {
    lightsurgeon::SceneDesc scene;
    MString err;
    const MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
    if (st != MS::kSuccess) return st;
    lightsurgeon::saveSceneFile(path.asChar(), scene);
    setResult(path);
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::matchHero(const MString& path, bool apply) {
    lightsurgeon::SceneDesc scene;
    MString err;
    MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
    if (st != MS::kSuccess) return st;
    const auto current = lightsurgeon::analyzeScene(scene);
    std::ifstream in(path.asChar());
    if (!in) return MS::kFailure;
    std::ostringstream ss;
    ss << in.rdbuf();
    const auto hero = lightsurgeon::analysisFromJson(lightsurgeon::Json::parse(ss.str()));
    const auto match = lightsurgeon::matchHero(current, hero);
    resultText_ = lightsurgeon::matchToJson(match).dump(2).c_str();
    setResult(resultText_);
    MGlobal::displayInfo(resultText_);
    if (!apply) return MS::kSuccess;
    action_ = Action::Scale;
    names_.clear();
    oldIntensity_.clear();
    newIntensity_.clear();
    for (const auto& scale : match.scales) {
        if (scale.role != "key" && scale.role != "fill" && scale.role != "rim") continue;
        double oldV = 0.0;
        if (getLightIntensity(scale.name.c_str(), &oldV) != MS::kSuccess) continue;
        names_.append(scale.name.c_str());
        oldIntensity_.append(oldV);
        newIntensity_.append(oldV * scale.intensityScale);
    }
    return redoIt();
}

MStatus LightSurgeonCmd::applyMuting(bool dead, bool noisy) {
    lightsurgeon::SceneDesc scene;
    MString err;
    const MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
    if (st != MS::kSuccess) return st;
    const auto result = lightsurgeon::analyzeScene(scene);
    action_ = Action::Mute;
    names_.clear();
    oldIntensity_.clear();
    newIntensity_.clear();
    for (const auto& light : result.lights) {
        if ((dead && light.dead) || (noisy && light.noisy)) {
            double oldV = 0.0;
            if (getLightIntensity(light.name.c_str(), &oldV) != MS::kSuccess) continue;
            names_.append(light.name.c_str());
            oldIntensity_.append(oldV);
            newIntensity_.append(0.0);
        }
    }
    return redoIt();
}

MStatus LightSurgeonCmd::applySolo(const MString& name) {
    lightsurgeon::SceneDesc scene;
    MString err;
    const MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
    if (st != MS::kSuccess) return st;
    action_ = Action::Solo;
    names_.clear();
    oldIntensity_.clear();
    newIntensity_.clear();
    for (const auto& light : scene.lights) {
        double oldV = 0.0;
        if (getLightIntensity(light.name.c_str(), &oldV) != MS::kSuccess) continue;
        names_.append(light.name.c_str());
        oldIntensity_.append(oldV);
        newIntensity_.append(light.name == name.asChar() ? oldV : 0.0);
    }
    return redoIt();
}

MStatus LightSurgeonCmd::restoreAll() {
    action_ = Action::Restore;
    names_.clear();
    oldIntensity_.clear();
    newIntensity_.clear();
    lightsurgeon::SceneDesc scene;
    MString err;
    const MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
    if (st != MS::kSuccess) return st;
    for (const auto& light : scene.lights) {
        MSelectionList list;
        if (list.add(light.name.c_str()) != MS::kSuccess) continue;
        MDagPath path;
        list.getDagPath(0, path);
        MFnDependencyNode node(path.node());
        MPlug saved = node.findPlug("ls_savedIntensity", true);
        MPlug cur = node.findPlug("intensity", true);
        if (saved.isNull() || cur.isNull()) continue;
        names_.append(light.name.c_str());
        oldIntensity_.append(cur.asDouble());
        newIntensity_.append(saved.asDouble());
    }
    return redoIt();
}

MStatus LightSurgeonCmd::applyScales() { return redoIt(); }

MStatus LightSurgeonCmd::redoIt() {
    if (action_ == Action::None) return MS::kSuccess;
    for (unsigned i = 0; i < names_.length(); ++i) {
        const MStatus st = setLightIntensity(names_[i], newIntensity_[i]);
        if (st != MS::kSuccess) return st;
    }
    setResult(static_cast<int>(names_.length()));
    return MS::kSuccess;
}

MStatus LightSurgeonCmd::undoIt() {
    for (unsigned i = 0; i < names_.length(); ++i) {
        const MStatus st = setLightIntensity(names_[i], oldIntensity_[i]);
        if (st != MS::kSuccess) return st;
    }
    return MS::kSuccess;
}

bool LightSurgeonCmd::isUndoable() const { return action_ != Action::None; }

MStatus LightSurgeonCmd::doIt(const MArgList& args) {
    MArgDatabase db(syntax(), args);
    const bool hud = !db.isFlagSet(kHudFlag) || db.flagArgumentBool(kHudFlag, 0);
    if (db.isFlagSet(kRestoreFlag)) return restoreAll();
    if (db.isFlagSet(kSoloFlag)) return applySolo(db.flagArgumentString(kSoloFlag, 0));
    if (db.isFlagSet(kMuteDeadFlag) || db.isFlagSet(kMuteNoisyFlag)) {
        return applyMuting(db.isFlagSet(kMuteDeadFlag), db.isFlagSet(kMuteNoisyFlag));
    }
    if (db.isFlagSet(kMatchFlag)) {
        return matchHero(db.flagArgumentString(kMatchFlag, 0), db.isFlagSet(kApplyFlag));
    }
    if (db.isFlagSet(kPixelCenterFlag)) {
        lightsurgeon::SceneDesc scene;
        MString err;
        const MStatus st = lightsurgeon::extractMayaScene(&scene, &err);
        if (st != MS::kSuccess) {
            if (err.length()) MGlobal::displayError(err);
            return st;
        }
        return runPixel(scene.camera.width / 2, scene.camera.height / 2);
    }
    if (db.isFlagSet(kPixelFlag)) {
        return runPixel(db.flagArgumentInt(kPixelFlag, 0), db.flagArgumentInt(kPixelFlag, 1));
    }
    if (db.isFlagSet(kReportFlag)) return writeReport(db.flagArgumentString(kReportFlag, 0));
    if (db.isFlagSet(kExportFlag)) return exportScene(db.flagArgumentString(kExportFlag, 0));
    int start = db.isFlagSet(kStartFlag) ? db.flagArgumentInt(kStartFlag, 0) : 0;
    int end = db.isFlagSet(kEndFlag) ? db.flagArgumentInt(kEndFlag, 0) : 0;
    int step = db.isFlagSet(kStepFlag) ? db.flagArgumentInt(kStepFlag, 0) : 1;
    const bool asJson = db.isFlagSet(kJsonFlag);
    return analyzeAndPrint(hud, asJson, start, end, step);
}
