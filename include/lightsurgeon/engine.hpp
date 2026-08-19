#pragma once

#include "lightsurgeon/types.hpp"

#include <string>
#include <vector>

namespace lightsurgeon {

AnalysisResult analyzeScene(const SceneDesc& scene, const AnalyzeSettings& settings = {});
PixelAutopsy autopsyPixel(const SceneDesc& scene, int x, int y, const AnalyzeSettings& settings = {});
MatchResult matchHero(const AnalysisResult& current, const AnalysisResult& hero);
AnalysisResult mergeAnalyses(const std::vector<AnalysisResult>& parts);
std::string formatReport(const AnalysisResult& result);
std::string formatHudReport(const AnalysisResult& result);

}  // namespace lightsurgeon
