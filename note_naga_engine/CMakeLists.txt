cmake_minimum_required(VERSION 3.16)
project(note_naga_engine LANGUAGES CXX)

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/note_naga_version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/note_naga_engine/note_naga_version.h
)

if(NOT QT_DEACTIVATED)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
else()
    set(CMAKE_AUTOUIC OFF)
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTORCC OFF)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(QT_DEACTIVATED "Build without Qt support" OFF)
message(STATUS "QT_DEACTIVATED = ${QT_DEACTIVATED}")

set(PUBLIC_HEADER_FILES
    # include/note_naga_engine
    ./include/note_naga_engine/logger.h
    ./include/note_naga_engine/note_naga_api.h
    ./include/note_naga_engine/note_naga_engine.h
    ./include/note_naga_engine/nn_utils.h
    # include/note_naga_engine/core
    ./include/note_naga_engine/core/soundfont_finder.h
    ./include/note_naga_engine/core/types.h
    ./include/note_naga_engine/core/dsp_block_base.h
    ./include/note_naga_engine/core/lock_free_spsc_queue.h
    ./include/note_naga_engine/core/lock_free_mpmc_queue.h
    ./include/note_naga_engine/core/async_queue_component.h
    ./include/note_naga_engine/core/project_data.h
    ./include/note_naga_engine/core/note_naga_synthesizer.h
    ./include/note_naga_engine/core/project_file_types.h
    ./include/note_naga_engine/core/project_serializer.h
    ./include/note_naga_engine/core/recent_projects_manager.h
    # include/note_naga_engine/io
    ./include/note_naga_engine/io/midi_file.h
    # include/note_naga_engine/module
    ./include/note_naga_engine/module/mixer.h
    ./include/note_naga_engine/module/playback_worker.h
    ./include/note_naga_engine/module/audio_worker.h
    ./include/note_naga_engine/module/dsp_engine.h
    ./include/note_naga_engine/module/metronome.h
    ./include/note_naga_engine/module/spectrum_analyzer.h
    ./include/note_naga_engine/module/pan_analyzer.h
    # include/note_naga_engine/synth
    ./include/note_naga_engine/synth/synth_fluidsynth.h
    ./include/note_naga_engine/synth/synth_external_midi.h
    # include/note_naga_engine/dsp
    ./include/note_naga_engine/dsp/dsp_block_gain.h
    ./include/note_naga_engine/dsp/dsp_block_pan.h
    ./include/note_naga_engine/dsp/dsp_block_single_eq.h
    ./include/note_naga_engine/dsp/dsp_block_compressor.h
    ./include/note_naga_engine/dsp/dsp_block_multi_eq.h
    ./include/note_naga_engine/dsp/dsp_block_limiter.h
    ./include/note_naga_engine/dsp/dsp_block_delay.h
    ./include/note_naga_engine/dsp/dsp_block_reverb.h
    ./include/note_naga_engine/dsp/dsp_block_bitcrusher.h
    ./include/note_naga_engine/dsp/dsp_block_tremolo.h
    ./include/note_naga_engine/dsp/dsp_block_filter.h
    ./include/note_naga_engine/dsp/dsp_block_chorus.h
    ./include/note_naga_engine/dsp/dsp_block_phaser.h
    ./include/note_naga_engine/dsp/dsp_block_flanger.h
    ./include/note_naga_engine/dsp/dsp_block_noise_gate.h
    ./include/note_naga_engine/dsp/dsp_block_saturator.h
    ./include/note_naga_engine/dsp/dsp_block_exciter.h
    ./include/note_naga_engine/dsp/dsp_block_stereo_imager.h
)

set(SOURCE_FILES
    # src
    ./logger.cpp
    ./note_naga_engine.cpp
    ./nn_utils.cpp
    # core
    ./core/soundfont_finder.cpp
    ./core/project_data.cpp
    ./core/types.cpp
    ./core/project_serializer.cpp
    ./core/recent_projects_manager.cpp
    # io
    ./io/midi_file.cpp
    # module
    ./module/mixer.cpp
    ./module/playback_worker.cpp
    ./module/audio_worker.cpp
    ./module/dsp_engine.cpp
    ./module/metronome.cpp
    ./module/spectrum_analyzer.cpp
    ./module/pan_analyzer.cpp
    # synth
    ./synth/synth_fluidsynth.cpp
    ./synth/synth_external_midi.cpp
    # dsp
    ./dsp/dsp_block_gain.cpp
    ./dsp/dsp_block_pan.cpp
    ./dsp/dsp_block_single_eq.cpp
    ./dsp/dsp_block_compressor.cpp
    ./dsp/dsp_block_multi_eq.cpp
    ./dsp/dsp_block_limiter.cpp
    ./dsp/dsp_block_delay.cpp
    ./dsp/dsp_block_reverb.cpp
    ./dsp/dsp_block_bitcrusher.cpp
    ./dsp/dsp_block_tremolo.cpp
    ./dsp/dsp_block_filter.cpp
    ./dsp/dsp_block_chorus.cpp
    ./dsp/dsp_block_phaser.cpp
    ./dsp/dsp_block_flanger.cpp
    ./dsp/dsp_block_noise_gate.cpp
    ./dsp/dsp_block_saturator.cpp
    ./dsp/dsp_block_exciter.cpp
    ./dsp/dsp_block_stereo_imager.cpp
)

# GLIB2, FluidSynth, and RtMidi dependencies (REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)
pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)
pkg_check_modules(RTAUDIO REQUIRED IMPORTED_TARGET rtaudio)

# Qt dependencies (OPTIONAL)
if(NOT QT_DEACTIVATED)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
    else()
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
    endif()
endif()

add_library(note_naga_engine STATIC
    ${PUBLIC_HEADER_FILES}
    ${SOURCE_FILES}
)

target_include_directories(note_naga_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

if(NOT QT_DEACTIVATED)
target_link_libraries(note_naga_engine
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        PkgConfig::GLIB2
        PkgConfig::FLUIDSYNTH
        PkgConfig::RTMIDI
        PkgConfig::RTAUDIO
)
else()
target_link_libraries(note_naga_engine
    PUBLIC
        PkgConfig::GLIB2
        PkgConfig::FLUIDSYNTH
        PkgConfig::RTMIDI
)
endif()

if(QT_DEACTIVATED)
    target_compile_definitions(note_naga_engine PUBLIC QT_DEACTIVATED)
endif()

install(TARGETS note_naga_engine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES ${PUBLIC_HEADER_FILES} DESTINATION include/note_naga_engine)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/note_naga_engine/note_naga_version.h DESTINATION include/note_naga_engine)