cmake_minimum_required(VERSION 3.16)

project(NoteNaga VERSION 0.8.0 LANGUAGES CXX)

set(MYAPP_VERSION_MAJOR 0)
set(MYAPP_VERSION_MINOR 8)
set(MYAPP_VERSION_PATCH 0)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent Svg)
else()
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent Svg)
endif()

set(RESOURCES src/resources.qrc)
qt6_add_resources(RESOURCES_RCC ${RESOURCES})

# Použijeme pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)
pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)

set(HEADER_FILES
    src/main_window.h

    src/core/midi_file.h
    src/core/shared.h
    src/core/app_context.h
    src/core/icons.h
    src/core/playback_worker.h
    src/core/mixer.h

    src/dialogs/instrument_selector_dialog.h   
    
    src/utils/space_action.h

    src/widgets/volume_bar.h
    src/widgets/multi_channel_volume_bar.h
    src/widgets/audio_dial.h
    src/widgets/audio_dial_centered.h
    src/widgets/animated_time_label.h

    src/main_widgets/midi_control_bar_widget.h
    src/main_widgets/track_widget.h
    src/main_widgets/track_list_widget.h
    src/main_widgets/midi_tact_ruler.h
    src/main_widgets/midi_keyboard_ruler.h
    src/main_widgets/midi_editor_widget.h
    src/main_widgets/routing_entry_widget.h
    src/main_widgets/track_mixer_widget.h
)

set(SOURCE_FILES
    src/main.cpp
    src/main_window.cpp
    
    src/core/midi_file.cpp
    src/core/shared.cpp
    src/core/app_context.cpp
    src/core/icons.cpp
    src/core/playback_worker.cpp
    src/core/mixer.cpp

    src/dialogs/instrument_selector_dialog.cpp

    src/utils/space_action.cpp

    src/widgets/volume_bar.cpp
    src/widgets/multi_channel_volume_bar.cpp
    src/widgets/audio_dial.cpp
    src/widgets/audio_dial_centered.cpp
    src/widgets/animated_time_label.cpp

    src/main_widgets/midi_control_bar_widget.cpp
    src/main_widgets/track_widget.cpp
    src/main_widgets/track_list_widget.cpp
    src/main_widgets/midi_tact_ruler.cpp
    src/main_widgets/midi_keyboard_ruler.cpp
    src/main_widgets/midi_editor_widget.cpp
    src/main_widgets/routing_entry_widget.cpp
    src/main_widgets/track_mixer_widget.cpp
)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(NoteNaga
        MANUAL_FINALIZATION
        ${HEADER_FILES} ${SOURCE_FILES} ${RESOURCES_RCC}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET untitled APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(NoteNaga SHARED
            ${HEADER_FILES} ${SOURCE_FILES} ${RESOURCES_RCC}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(NoteNaga
            ${HEADER_FILES} ${SOURCE_FILES} ${RESOURCES_RCC}
        )
    endif()
endif()

# Všechno linkování pomocí pkg-config targetů
target_link_libraries(NoteNaga PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Svg
    PkgConfig::GLIB2
    PkgConfig::FLUIDSYNTH
    PkgConfig::RTMIDI
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.saucom.stork)
endif()
set_target_properties(NoteNaga PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS NoteNaga
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NoteNaga)
endif()
