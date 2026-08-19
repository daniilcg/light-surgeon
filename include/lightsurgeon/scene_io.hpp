#pragma once

#include "lightsurgeon/json.hpp"
#include "lightsurgeon/types.hpp"

#include <string>

namespace lightsurgeon {

SceneDesc sceneFromJson(const Json& root);
Json sceneToJson(const SceneDesc& scene);
SceneDesc loadSceneFile(const std::string& path);
void saveSceneFile(const std::string& path, const SceneDesc& scene);

Json analysisToJson(const AnalysisResult& result);
Json autopsyToJson(const PixelAutopsy& autopsy);
Json matchToJson(const MatchResult& match);
AnalysisResult analysisFromJson(const Json& root);

}  // namespace lightsurgeon
