#pragma once

#include <QColor>
#include <QFont>
#include <QPen>
#include <QRect>
#include <QWidget>

#include <note_naga_engine/note_naga_engine.h>

/**
 * @brief The MidiTactRuler class provides a visual representation of the MIDI tact ruler.
 * It displays the current position in ticks and allows interaction to select a position.
 */
class MidiTactRuler : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructor for MidiTactRuler.
     * @param engine Pointer to the NoteNagaEngine instance.
     * @param parent Parent widget.
     */
    explicit MidiTactRuler(NoteNagaEngine *engine, QWidget *parent = nullptr);

    /**
     * @brief Sets the time scale for the ruler.
     * @param time_scale The new time scale value.
     */
    void setTimeScale(double time_scale);

    // Colors
    QColor bg_color;
    QColor fg_color;
    QColor subline_color;
    QColor tact_bg_color;
    QColor tact_line_color;
    QColor hover_color;
    QColor click_hint_color;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    NoteNagaEngine *engine;

    double time_scale;
    int horizontalScroll;
    QFont font;
    
    // Hover state
    bool m_isHovered = false;
    int m_hoverX = -1;

/*******************************************************************************************************/
// Signal and Slots
/*******************************************************************************************************/
public slots:
    /**
     * @brief Sets the horizontal scroll position. Must be same as the horizontal scroll
     * bar value for MIDI editor.
     * @param val The new horizontal scroll value.
     */
    void setHorizontalScroll(int val);

signals:
    /**
     * @brief Signal emitted when a position is selected.
     * @param tick The tick position selected.
     */
    void positionSelected(int tick);
    
};