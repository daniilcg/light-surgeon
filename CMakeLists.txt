cmake_minimum_required(VERSION 3.16)
project(LightSurgeon VERSION 1.0.0 LANGUAGES CXX
    DESCRIPTION "Maya lighting surgeon by Dan Segal"
    HOMEPAGE_URL "https://www.linkedin.com/in/daniilcg/")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(lightsurgeon_core STATIC
    src/core/json.cpp
    src/core/bvh.cpp
    src/core/engine.cpp
    src/core/scene_io.cpp
)
target_include_directories(lightsurgeon_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(lightsurgeon_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(lightsurgeon_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(lightsurgeon src/cli/main.cpp)
target_link_libraries(lightsurgeon PRIVATE lightsurgeon_core)

add_executable(lightsurgeon_tests tests/test_lightsurgeon.cpp)
target_link_libraries(lightsurgeon_tests PRIVATE lightsurgeon_core)
target_compile_definitions(lightsurgeon_tests PRIVATE
    "LS_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures\""
)

enable_testing()
add_test(NAME lightsurgeon_unit COMMAND lightsurgeon_tests)
add_test(NAME lightsurgeon_cli_analyze COMMAND lightsurgeon analyze "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/studio.json")
add_test(NAME lightsurgeon_cli_pixel COMMAND lightsurgeon pixel "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/studio.json" 960 540)

include(FindMaya)
if(MAYA_FOUND)
    add_library(lightSurgeon MODULE
        src/maya/pluginMain.cpp
        src/maya/lightSurgeonCmd.cpp
        src/maya/lightSurgeonHud.cpp
        src/maya/sceneExtractor.cpp
    )
    target_link_libraries(lightSurgeon PRIVATE lightsurgeon_core Maya::Maya)
    target_include_directories(lightSurgeon PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/maya")
    set_target_properties(lightSurgeon PROPERTIES
        PREFIX ""
        SUFFIX "${MAYA_PLUGIN_SUFFIX}"
    )
    if(WIN32)
        set_target_properties(lightSurgeon PROPERTIES LINK_FLAGS "/export:initializePlugin /export:uninitializePlugin")
    endif()
    message(STATUS "Maya found at ${MAYA_LOCATION}; plugin target enabled")
else()
    message(STATUS "Maya SDK not found; building core, CLI, and tests only. Set MAYA_LOCATION to build the .mll/.so plugin.")
endif()

install(TARGETS lightsurgeon RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
install(FILES modules/lightSurgeon.mod DESTINATION .)
install(FILES scripts/AElightSurgeonHudTemplate.mel DESTINATION scripts)
if(MAYA_FOUND)
    install(TARGETS lightSurgeon DESTINATION plug-ins)
endif()
