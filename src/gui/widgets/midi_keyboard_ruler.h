#pragma once

#include <QColor>
#include <QFont>
#include <QMap>
#include <QMouseEvent>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <optional>
#include <vector>

#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief The MidiKeyboardRuler class provides a visual representation of a MIDI keyboard.
 * It allows interaction with MIDI notes, including playing and stopping notes,
 * and visual feedback for pressed and hovered keys.
 */
class MidiKeyboardRuler : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructor for MidiKeyboardRuler.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param viewer_row_height Height of each row in the viewer.
     * @param parent Parent widget.
     */
    explicit MidiKeyboardRuler(NoteNagaEngine *engine, int viewer_row_height = 16,
                               QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // Colors
    QColor bg_color;
    QColor white_key_color;
    QColor black_key_color;
    QColor white_key_line_color;
    QColor black_key_line_color;
    QColor hover_color;
    QColor press_color;
    QColor c_key_label_color;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    NoteNagaEngine *engine;

    int viewer_row_height;
    int verticalScroll;

    NN_Note_t pressed_note;
    int hovered_note;

    QFont font;
    QFont c_key_font;

    QMap<int, QColor> key_highlights;
    QMap<int, QTimer *> highlight_timers;

    std::vector<int> white_keys() const;
    std::vector<int> black_keys() const;
    std::optional<int> note_at_pos(const QPoint &pos) const;

    void highlightKey(int note, const QColor &color, int timeout);
    void removeHighlight(int note);

    static bool isBlackKey(int midi_note);
    static bool isWhiteKey(int midi_note);
    static constexpr int WHITE_ORDER[7] = {0, 2, 4, 5, 7, 9, 11};
    static constexpr int BLACK_ORDER[5] = {1, 3, 6, 8, 10};
    static constexpr double WHITE_HEIGHT[12] = {1.5, 0.0, 2.0, 0.0, 1.5, 1.5,
                                                0.0, 2.0, 0.0, 2.0, 0.0, 1.5};

/*******************************************************************************************************/
// Signal and Slots
/*******************************************************************************************************/

signals:
    /**
     * @brief Signal emitted when a note is pressed.
     * @param note The NN_Note_t that was pressed.
     */
    void notePressed(const NN_Note_t &note);

    /**
     * @brief Signal emitted when a note is released.
     * @param note The NN_Note_t that was released.
     */
    void noteReleased(const NN_Note_t &note);

public slots:
    /**
     * @brief Slot to handle playing a note. Highlights the key with a color of note
     * track.
     * @param note The NN_Note_t to play.
     */
    void handleNotePlay(const NN_Note_t &note);

    /**
     * @brief Slot to clear all key highlights.
     */
    void clearHighlights();

    /**
     * @brief Slot to set the vertical scroll position.
     * @param v Vertical scroll position.
     */
    void setRowHeight(int height);

    /**
     * @brief Slot to set the vertical scroll position and row height.
     * @param value Vertical scroll position.
     */
    void setVerticalScroll(float value);
};