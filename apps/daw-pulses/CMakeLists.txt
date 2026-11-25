# Fuzzy John DAW Pulses — application Qt 6 (premier squelette)
# Licence : GPLv3.
#
# Utilise :
#   - libs/fuzzy-control  : la Maschine MK3 / Maschine+ (pads, lumieres, ecrans)
#   - prototypes/pad-sound : le moteur audio JUCE (programme a part, pilote en OSC)
cmake_minimum_required(VERSION 3.22)
project(FuzzyDawPulses VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)                 # images de l'interface (resources/resources.qrc)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network)   # Mac : brew install qt

# La bibliotheque de la Maschine, compilee avec l'app (ses outils de test ne sont pas compiles)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../libs/fuzzy-control
                 ${CMAKE_CURRENT_BINARY_DIR}/fuzzy-control EXCLUDE_FROM_ALL)

qt_add_executable(fuzzy-daw-pulses MACOSX_BUNDLE
    src/main.cpp
    resources/resources.qrc
    src/main_window.cpp src/main_window.h
    src/engine_link.cpp src/engine_link.h
    src/maschine_link.cpp src/maschine_link.h
    src/transport.cpp src/transport.h
    src/pad_grid.cpp src/pad_grid.h
    src/piano_keys.cpp src/piano_keys.h
    src/osc.cpp src/osc.h
    src/system_stats.cpp src/system_stats.h
    src/mixer_view.cpp src/mixer_view.h
    src/sequencer.cpp src/sequencer.h
    src/pattern_view.cpp src/pattern_view.h
    src/project.cpp src/project.h
    src/loop_dialog.cpp src/loop_dialog.h
    src/piano_roll.cpp src/piano_roll.h
    src/generate_dialog.cpp src/generate_dialog.h
    src/midi_import.cpp src/midi_import.h
    src/keyboard_view.cpp src/keyboard_view.h src/music_tables.h
    src/note_dialog.cpp src/note_dialog.h
    src/settings_dialog.cpp src/settings_dialog.h
    src/session.h)

set_target_properties(fuzzy-daw-pulses PROPERTIES
    OUTPUT_NAME "Fuzzy DAW Pulses"
    MACOSX_BUNDLE_BUNDLE_NAME "Fuzzy DAW Pulses"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.fuzzyturtle.dawpulses"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

target_link_libraries(fuzzy-daw-pulses PRIVATE Qt6::Widgets Qt6::Network fuzzy_control)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(fuzzy-daw-pulses PRIVATE -Wall -Wextra)
endif()
