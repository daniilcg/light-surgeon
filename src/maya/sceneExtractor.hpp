#pragma once

#include "lightsurgeon/types.hpp"

#include <maya/MStatus.h>
#include <maya/MString.h>

namespace lightsurgeon {

MStatus extractMayaScene(SceneDesc* scene, MString* error);

}  // namespace lightsurgeon
