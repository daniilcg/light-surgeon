#include "lightsurgeon/engine.hpp"
#include "lightsurgeon/scene_io.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void printUsage() {
    std::cout
        << "Light Surgeon — lighting contribution surgeon for Maya / batch\n"
        << "Created by Dan Segal (https://www.linkedin.com/in/daniilcg/)\n"
        << "Usage:\n"
        << "  lightsurgeon analyze <scene.json> [-o report.json]\n"
        << "  lightsurgeon pixel <scene.json> <x> <y>\n"
        << "  lightsurgeon match <current.json> <hero.json>\n"
        << "  lightsurgeon portals <scene.json>\n";
}

int writeOut(const std::string& path, const std::string& text) {
    if (path.empty()) {
        std::cout << text << std::endl;
        return 0;
    }
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) {
        std::cerr << "Cannot write " << path << "\n";
        return 2;
    }
    out << text;
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        return argc == 1 ? 0 : 1;
    }
    const std::string cmd = argv[1];
    try {
        if (cmd == "analyze") {
            const auto scene = lightsurgeon::loadSceneFile(argv[2]);
            const auto result = lightsurgeon::analyzeScene(scene);
            std::string outPath;
            for (int i = 3; i + 1 < argc; ++i) {
                if (std::strcmp(argv[i], "-o") == 0) outPath = argv[i + 1];
            }
            const std::string json = lightsurgeon::analysisToJson(result).dump(2);
            if (outPath.empty()) {
                std::cout << lightsurgeon::formatReport(result) << std::endl;
                std::cout << json << std::endl;
                return 0;
            }
            return writeOut(outPath, json);
        }
        if (cmd == "pixel") {
            if (argc < 5) {
                printUsage();
                return 1;
            }
            const auto scene = lightsurgeon::loadSceneFile(argv[2]);
            const int x = std::atoi(argv[3]);
            const int y = std::atoi(argv[4]);
            const auto autopsy = lightsurgeon::autopsyPixel(scene, x, y);
            std::cout << lightsurgeon::autopsyToJson(autopsy).dump(2) << std::endl;
            return 0;
        }
        if (cmd == "match") {
            if (argc < 4) {
                printUsage();
                return 1;
            }
            const auto current = lightsurgeon::analysisFromJson(lightsurgeon::Json::parse(
                [&]() {
                    std::ifstream in(argv[2]);
                    std::ostringstream ss;
                    ss << in.rdbuf();
                    return ss.str();
                }()));
            const auto hero = lightsurgeon::analysisFromJson(lightsurgeon::Json::parse(
                [&]() {
                    std::ifstream in(argv[3]);
                    std::ostringstream ss;
                    ss << in.rdbuf();
                    return ss.str();
                }()));
            const auto match = lightsurgeon::matchHero(current, hero);
            std::cout << lightsurgeon::matchToJson(match).dump(2) << std::endl;
            return 0;
        }
        if (cmd == "portals") {
            const auto scene = lightsurgeon::loadSceneFile(argv[2]);
            const auto result = lightsurgeon::analyzeScene(scene);
            lightsurgeon::Json arr = lightsurgeon::Json::array();
            const auto full = lightsurgeon::analysisToJson(result);
            std::cout << full.at("portals").dump(2) << std::endl;
            return 0;
        }
        printUsage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "lightsurgeon: " << ex.what() << "\n";
        return 2;
    }
}
