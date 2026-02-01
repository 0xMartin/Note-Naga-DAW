#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsSimpleTextItem>
#include <note_naga_engine/note_naga_engine.h>

/** MIDI editor follow modes */
enum class MidiEditorFollowMode {
    None,
    LeftSideIsCurrent,
    CenterIsCurrent,
    StepByStep
};

/** Note duration values for the note size combo box */
enum class NoteDuration {
    Whole,      // 1/1
    Half,       // 1/2
    Quarter,    // 1/4
    Eighth,     // 1/8
    Sixteenth,  // 1/16
    ThirtySecond // 1/32
};

/** Snap/grid resolution for note quantization */
enum class GridResolution {
    Whole,      // 1/1
    Half,       // 1/2
    Quarter,    // 1/4
    Eighth,     // 1/8
    Sixteenth,  // 1/16
    ThirtySecond, // 1/32
    Off         // No snap
};

/** Note color mode for visualization */
enum class NoteColorMode {
    TrackColor,  // Use track's color
    Velocity,    // Color based on velocity
    Pan          // Color based on pan position
};

/** Configuration for the MIDI editor */
struct MidiEditorConfig {
    double time_scale = 0.2;
    int key_height = 16;
    int tact_subdiv = 4;
    bool looping = false;
    MidiEditorFollowMode follow_mode = MidiEditorFollowMode::CenterIsCurrent;
    NoteColorMode color_mode = NoteColorMode::TrackColor;
};

/** Graphics representation of a note */
struct NoteGraphics {
    QGraphicsItem *item = nullptr;           // Graphic object for the note
    QGraphicsSimpleTextItem *label = nullptr; // Text label for the note
    NN_Note_t note;                          // Note data
    NoteNagaTrack *track = nullptr;          // Track the note belongs to
};

/** Drag state for mouse operations */
enum class NoteDragMode {
    None,       // No drag operation
    Select,     // Rubber band selection
    Move,       // Moving notes
    Resize      // Resizing notes
};

/** Color scheme for the MIDI editor */
struct MidiEditorColors {
    QColor bg_color{"#32353c"};
    QColor fg_color{"#e0e6ef"};
    QColor line_color{"#232731"};
    QColor subline_color{"#464a56"};
    QColor grid_bar_color{"#6177d1"};
    QColor grid_row_color1{"#35363b"};
    QColor grid_row_color2{"#292a2e"};
    QColor grid_bar_label_color{"#6fa5ff"};
    QColor grid_subdiv_color{"#44464b"};
    QColor selection_color{"#70a7ff"};
};
