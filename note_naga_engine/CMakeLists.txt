cmake_minimum_required(VERSION 3.16)
project(note_naga_engine LANGUAGES CXX)

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

set(HEADER_FILES
    note_naga_api.h
    note_naga_engine.h
    core/mixer.h
    core/project_data.h
    core/types.h
    io/midi_file.h
    worker/playback_worker.h
)

set(SOURCE_FILES
    note_naga_engine.cpp
    core/mixer.cpp
    core/project_data.cpp
    core/types.cpp
    io/midi_file.cpp
    worker/playback_worker.cpp
)

# GLIB2, FluidSynth, and RtMidi dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)
pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)

if(NOT QT_DEACTIVATED)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
    else()
        find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)
    endif()
endif()

add_library(note_naga_engine STATIC
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

target_include_directories(note_naga_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
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

if(NOT QT_DEACTIVATED)
    target_link_libraries(note_naga_engine PUBLIC Qt${QT_VERSION_MAJOR}::Core)
    set_target_properties(note_naga_engine PROPERTIES AUTOMOC ON)
    get_target_property(QTCORE_INCLUDE_DIRS Qt${QT_VERSION_MAJOR}::Core INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Qt${QT_VERSION_MAJOR}::Core include dirs: ${QTCORE_INCLUDE_DIRS}")
    target_include_directories(note_naga_engine PUBLIC ${QTCORE_INCLUDE_DIRS})
else()
    set_target_properties(note_naga_engine PROPERTIES AUTOMOC OFF)
endif()

if(QT_DEACTIVATED)
    target_compile_definitions(note_naga_engine PUBLIC QT_DEACTIVATED)
endif()

install(TARGETS note_naga_engine
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES ${HEADER_FILES} DESTINATION include/note_naga_engine)