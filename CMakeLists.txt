cmake_minimum_required(VERSION 3.16)

project(NoteNaga VERSION 0.9.0 LANGUAGES CXX)

set(MYAPP_VERSION_MAJOR 0)
set(MYAPP_VERSION_MINOR 9)
set(MYAPP_VERSION_PATCH 0)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Verovio - Music notation rendering library (git submodule in libs/)
set(BUILD_AS_LIBRARY ON CACHE BOOL "" FORCE)
set(NO_HUMDRUM_SUPPORT ON CACHE BOOL "" FORCE)
set(NO_ABC_SUPPORT ON CACHE BOOL "" FORCE)
set(NO_PAE_SUPPORT ON CACHE BOOL "" FORCE)
set(NO_DARMS_SUPPORT ON CACHE BOOL "" FORCE)
add_subdirectory(libs/verovio/cmake)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent Svg SvgWidgets PrintSupport Xml)
else()
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent Svg PrintSupport Xml)
endif()

find_package(OpenCV REQUIRED)

set(RESOURCES src/resources.qrc)
qt6_add_resources(RESOURCES_RCC ${RESOURCES})

# Engine
add_subdirectory(note_naga_engine)

set(HEADER_FILES
    # gui
    src/gui/main_window.h
    src/gui/nn_gui_utils.h
    # media export
    src/media_export/preview_worker.h
    src/media_export/media_exporter.h
    src/media_export/media_renderer.h
    # gui/sections
    src/gui/sections/section_interface.h
    src/gui/sections/section_switcher.h
    src/gui/sections/midi_editor_section.h
    src/gui/sections/dsp_editor_section.h
    src/gui/sections/media_export_section.h
    src/gui/sections/notation_section.h
    src/gui/sections/track_preview_widget.h
    src/gui/sections/project_section.h
    src/gui/sections/arrangement_section.h
    # gui/components
    src/gui/components/audio_dial.h
    src/gui/components/audio_dial_centered.h
    src/gui/components/audio_dial_grid_widget.h
    src/gui/components/audio_vertical_slider.h
    src/gui/components/button_group_widget.h
    src/gui/components/indicator_led_widget.h
    src/gui/components/midi_seq_progress_bar.h
    src/gui/components/midi_sequence_selector.h
    src/gui/components/separator.h
    src/gui/components/spectrum_analyzer.h
    src/gui/components/audio_bars_visualizer.h
    src/gui/components/stereo_volume_bar_widget.h
    src/gui/components/track_stereo_meter.h
    src/gui/components/vertical_combo_box.h
    src/gui/components/vertical_label.h
    # gui/dialogs
    src/gui/dialogs/dsp_block_chooser_dialog.h
    src/gui/dialogs/instrument_selector_dialog.h
    src/gui/dialogs/project_wizard_dialog.h
    src/gui/dialogs/track_settings_dialog.h
    # gui/dock_system
    src/gui/dock_system/advanced_dock_widget.h
    src/gui/dock_system/dock_indicator_overlay.h
    src/gui/dock_system/dock_title_bar.h
    # gui/editor
    src/gui/editor/midi_editor_widget.h
    src/gui/editor/midi_editor_types.h
    src/gui/editor/midi_editor_context_menu.h
    src/gui/editor/midi_editor_note_handler.h
    src/gui/editor/arrangement_layer_manager.h
    src/gui/editor/arrangement_timeline_widget.h
    src/gui/editor/arrangement_timeline_ruler.h
    src/gui/editor/arrangement_resource_panel.h
    src/gui/editor/arrangement_track_header_widget.h
    src/gui/editor/arrangement_track_headers_widget.h
    src/gui/editor/arrangement_minimap_widget.h
    # gui/widgets
    src/gui/widgets/verovio_widget.h
    src/gui/widgets/dsp_block_widget.h
    src/gui/widgets/dsp_engine_widget.h
    src/gui/widgets/global_transport_bar.h
    src/gui/widgets/midi_keyboard_ruler.h
    src/gui/widgets/midi_tact_ruler.h
    src/gui/widgets/track_list_widget.h
    src/gui/widgets/track_widget.h
)

set(SOURCE_FILES
    # src
    src/main.cpp
    # media export
    src/media_export/preview_worker.cpp
    src/media_export/media_exporter.cpp
    src/media_export/media_renderer.cpp
    # gui
    src/gui/main_window.cpp
    src/gui/nn_gui_utils.cpp
    # gui/sections
    src/gui/sections/section_switcher.cpp
    src/gui/sections/midi_editor_section.cpp
    src/gui/sections/dsp_editor_section.cpp
    src/gui/sections/media_export_section.cpp
    src/gui/sections/notation_section.cpp
    src/gui/sections/track_preview_widget.cpp
    src/gui/sections/project_section.cpp
    src/gui/sections/arrangement_section.cpp
    # gui/components
    src/gui/components/audio_dial.cpp
    src/gui/components/audio_dial_centered.cpp
    src/gui/components/audio_dial_grid_widget.cpp
    src/gui/components/audio_vertical_slider.cpp
    src/gui/components/button_group_widget.cpp
    src/gui/components/indicator_led_widget.cpp
    src/gui/components/midi_seq_progress_bar.cpp
    src/gui/components/midi_sequence_selector.cpp
    src/gui/components/pan_analyzer.cpp
    src/gui/components/separator.cpp
    src/gui/components/spectrum_analyzer.cpp
    src/gui/components/audio_bars_visualizer.cpp
    src/gui/components/stereo_volume_bar_widget.cpp
    src/gui/components/track_stereo_meter.cpp
    src/gui/components/vertical_combo_box.cpp
    src/gui/components/vertical_label.cpp
    # gui/dialogs
    src/gui/dialogs/dsp_block_chooser_dialog.cpp
    src/gui/dialogs/instrument_selector_dialog.cpp
    src/gui/dialogs/project_wizard_dialog.cpp
    src/gui/dialogs/track_settings_dialog.cpp
    # gui/dock_system
    src/gui/dock_system/advanced_dock_widget.cpp
    src/gui/dock_system/dock_indicator_overlay.cpp
    src/gui/dock_system/dock_title_bar.cpp
    # gui/editor
    src/gui/editor/midi_editor_widget.cpp
    src/gui/editor/midi_editor_context_menu.cpp
    src/gui/editor/midi_editor_note_handler.cpp
    src/gui/editor/note_property_editor.cpp
    src/gui/editor/tempo_track_editor.cpp
    src/gui/editor/arrangement_layer_manager.cpp
    src/gui/editor/arrangement_timeline_widget.cpp
    src/gui/editor/arrangement_timeline_ruler.cpp
    src/gui/editor/arrangement_resource_panel.cpp
    src/gui/editor/arrangement_track_header_widget.cpp
    src/gui/editor/arrangement_track_headers_widget.cpp
    src/gui/editor/arrangement_minimap_widget.cpp
    # gui/undo
    src/gui/undo/undo_manager.cpp
    src/gui/undo/midi_note_commands.cpp
    # gui/widgets
    src/gui/widgets/verovio_widget.cpp
    src/gui/widgets/dsp_block_widget.cpp
    src/gui/widgets/dsp_engine_widget.cpp
    src/gui/widgets/global_transport_bar.cpp
    src/gui/widgets/midi_keyboard_ruler.cpp
    src/gui/widgets/midi_tact_ruler.cpp
    src/gui/widgets/timeline_overview_widget.cpp
    src/gui/widgets/track_list_widget.cpp
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

target_link_libraries(NoteNaga PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::SvgWidgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Xml
    ${OpenCV_LIBS}
    note_naga_engine
    verovio
)

target_include_directories(NoteNaga PRIVATE 
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/libs/verovio/include
    ${CMAKE_SOURCE_DIR}/libs/verovio/include/vrv
    ${CMAKE_SOURCE_DIR}/libs/verovio/include/json
    ${CMAKE_SOURCE_DIR}/libs/verovio/include/midi
    ${CMAKE_SOURCE_DIR}/libs/verovio/include/pugi
    ${CMAKE_SOURCE_DIR}/libs/verovio/libmei/dist
    ${CMAKE_SOURCE_DIR}/libs/verovio/libmei/addons
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