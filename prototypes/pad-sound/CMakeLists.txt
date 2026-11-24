# Fuzzy Turtle - prototype 4 : un son JUCE au toucher d'un pad Maschine
cmake_minimum_required(VERSION 3.22)
project(FuzzyPadSound VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE est telecharge automatiquement depuis GitHub au premier "cmake" (licence AGPLv3/commerciale ;
# notre code GPLv3 l'utilise sous AGPLv3, compatible).
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# hidapi (MacPorts sur Mac : "hidapi" ; Linux : "hidapi-hidraw")
find_package(PkgConfig REQUIRED)
pkg_search_module(HIDAPI REQUIRED IMPORTED_TARGET hidapi hidapi-hidraw)

juce_add_console_app(fuzzy_pad_sound PRODUCT_NAME "Fuzzy Pad Sound")
target_sources(fuzzy_pad_sound PRIVATE main.cpp)
target_compile_definitions(fuzzy_pad_sound PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1)   # MP3 lu directement (sur Mac, le decodeur du systeme le lit deja)
target_link_libraries(fuzzy_pad_sound
    PRIVATE juce::juce_audio_devices juce::juce_audio_basics juce::juce_audio_formats juce::juce_events juce::juce_osc PkgConfig::HIDAPI
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Test du moteur sans carte son (cmake -DFUZZY_ENGINE_TESTS=ON) : notes tres rapides -> silence a la fin
option(FUZZY_ENGINE_TESTS "Compiler les tests du moteur" OFF)
if(FUZZY_ENGINE_TESTS)
    juce_add_console_app(engine_tests PRODUCT_NAME "Engine Tests")
    target_sources(engine_tests PRIVATE tests/stuck_notes_test.cpp)
    target_compile_definitions(engine_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1)   # MP3 lu directement (sur Mac, le decodeur du systeme le lit deja)
    target_link_libraries(engine_tests
        PRIVATE juce::juce_audio_devices juce::juce_audio_basics juce::juce_audio_formats juce::juce_events juce::juce_osc PkgConfig::HIDAPI
        PUBLIC juce::juce_recommended_config_flags)
endif()
