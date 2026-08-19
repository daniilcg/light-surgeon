#include "lightSurgeonCmd.hpp"
#include "lightSurgeonHud.hpp"

#include <maya/MDrawRegistry.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

MStatus initializePlugin(MObject obj) {
    MFnPlugin plugin(obj, "Dan Segal", "1.0.0", "Any");
    MStatus st = plugin.registerCommand("lightSurgeon", LightSurgeonCmd::creator, LightSurgeonCmd::newSyntax);
    if (!st) return st;
    st = plugin.registerNode("lightSurgeonHud", LightSurgeonHud::id, LightSurgeonHud::creator,
                             LightSurgeonHud::initialize, MPxNode::kLocatorNode,
                             &LightSurgeonHud::drawDbClassification);
    if (!st) return st;
    st = MHWRender::MDrawRegistry::registerDrawOverrideCreator(
        LightSurgeonHud::drawDbClassification, LightSurgeonHud::drawRegistrantId, LightSurgeonHudOverride::creator);
    if (!st) return st;
    MGlobal::executeCommand(
        "if (!`menu -exists LightSurgeonMenu`) {"
        "menu -parent MayaWindow -label \"Light Surgeon\" LightSurgeonMenu;"
        "menuItem -label \"Analyze Shot\" -command \"lightSurgeon -analyze\";"
        "menuItem -label \"Mute Dead Lights\" -command \"lightSurgeon -muteDead\";"
        "menuItem -label \"Mute Noisy Lights\" -command \"lightSurgeon -muteNoisy\";"
        "menuItem -label \"Restore Intensities\" -command \"lightSurgeon -restore\";"
        "menuItem -label \"Write Report\" -command \"lightSurgeon -report lightSurgeon_report.json\";"
        "}");
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj) {
    MFnPlugin plugin(obj);
    MHWRender::MDrawRegistry::deregisterDrawOverrideCreator(LightSurgeonHud::drawDbClassification,
                                                            LightSurgeonHud::drawRegistrantId);
    plugin.deregisterNode(LightSurgeonHud::id);
    plugin.deregisterCommand("lightSurgeon");
    MGlobal::executeCommand("if (`menu -exists LightSurgeonMenu`) deleteUI LightSurgeonMenu;");
    return MS::kSuccess;
}
