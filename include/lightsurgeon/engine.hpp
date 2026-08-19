#pragma once

#include "lightsurgeon/types.hpp"

namespace lightsurgeon {

AnalysisResult analyzeScene(const SceneDesc& scene, const AnalyzeSettings& settings = {});
PixelAutopsy autopsyPixel(const SceneDesc& scene, int x, int y, const AnalyzeSettings& settings = {});
MatchResult matchHero(const AnalysisResult& current, const AnalysisResult& hero);
std::string formatReport(const AnalysisResult& result);

}  // namespace lightsurgeon
