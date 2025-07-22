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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(QT_DEACTIVATED "Build without Qt support" OFF)
message(STATUS "QT_DEACTIVATED = ${QT_DEACTIVATED}")

set(PUBLIC_HEADER_FILES
    # include/note_naga_engine
    ./include/note_naga_engine/logger.h
    ./include/note_naga_engine/note_naga_api.h
    ./include/note_naga_engine/note_naga_engine.h
    # include/note_naga_engine/core
    ./include/note_naga_engine/core/lock_free_spsc_queue.h
    ./include/note_naga_engine/core/lock_free_mpmc_queue.h
    ./include/note_naga_engine/core/note_naga_component.h
    ./include/note_naga_engine/core/project_data.h
    ./include/note_naga_engine/core/types.h
    # include/note_naga_engine/io
    ./include/note_naga_engine/io/midi_file.h
    # include/note_naga_engine/module
    ./include/note_naga_engine/module/mixer.h
    ./include/note_naga_engine/module/playback_worker.h
)

set(SOURCE_FILES
    # src
    ./logger.cpp
    ./note_naga_engine.cpp
    # core
    ./core/project_data.cpp
    ./core/types.cpp
    # io
    ./io/midi_file.cpp
    # module
    ./module/mixer.cpp
    ./module/playback_worker.cpp
)

# GLIB2, FluidSynth, and RtMidi dependencies (REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)
pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)

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