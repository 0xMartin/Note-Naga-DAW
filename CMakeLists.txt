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

# --- Přidej note_naga_engine knihovnu ---
add_subdirectory(note_naga_engine)

set(HEADER_FILES
    # gui
    src/gui/icons.h
    src/gui/main_window.h
    # gui/components
    src/gui/components/animated_time_label.h
    src/gui/components/audio_dial.h
    src/gui/components/audio_dial_centered.h
    src/gui/components/indicator_led_widget.h
    src/gui/components/multi_channel_volume_bar.h
    src/gui/components/volume_bar.h
    # gui/dialogs
    src/gui/dialogs/instrument_selector_dialog.h
    # gui/widgets
    src/gui/widgets/midi_control_bar_widget.h
    src/gui/widgets/midi_editor_widget.h
    src/gui/widgets/midi_keyboard_ruler.h
    src/gui/widgets/midi_tact_ruler.h
    src/gui/widgets/routing_entry_widget.h
    src/gui/widgets/track_list_widget.h
    src/gui/widgets/track_mixer_widget.h
    src/gui/widgets/track_widget.h
)

set(SOURCE_FILES
    # src
    src/main.cpp
    # gui
    src/gui/icons.cpp
    src/gui/main_window.cpp
    # gui/components
    src/gui/components/animated_time_label.cpp
    src/gui/components/audio_dial.cpp
    src/gui/components/audio_dial_centered.cpp
    src/gui/components/indicator_led_widget.cpp
    src/gui/components/multi_channel_volume_bar.cpp
    src/gui/components/volume_bar.cpp
    # gui/dialogs
    src/gui/dialogs/instrument_selector_dialog.cpp
    # gui/widgets
    src/gui/widgets/midi_control_bar_widget.cpp
    src/gui/widgets/midi_editor_widget.cpp
    src/gui/widgets/midi_keyboard_ruler.cpp
    src/gui/widgets/midi_tact_ruler.cpp
    src/gui/widgets/routing_entry_widget.cpp
    src/gui/widgets/track_list_widget.cpp
    src/gui/widgets/track_mixer_widget.cpp
    src/gui/widgets/track_widget.cpp
)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(NoteNaga
        MANUAL_FINALIZATION
        ${HEADER_FILES} ${SOURCE_FILES} ${RESOURCES_RCC}
    )
else()
    if(ANDROID)
        add_library(NoteNaga SHARED
            ${HEADER_FILES} ${SOURCE_FILES} ${RESOURCES_RCC}
        )
    else()
        add_executable(NoteNaga
            ${HEADER_FILES} ${SOURCE_FILES} ${RESOURCES_RCC}
        )
    endif()
endif()

target_include_directories(NoteNaga PRIVATE ${CMAKE_SOURCE_DIR}/note_naga_engine)

target_link_libraries(NoteNaga PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Svg
    note_naga_engine
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
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