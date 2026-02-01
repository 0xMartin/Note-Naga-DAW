#include "midi_tact_ruler.h"

#include <QPainter>
#include <QMouseEvent>
#include <QRect>
#include <QPen>

MidiTactRuler::MidiTactRuler(NoteNagaEngine *engine_, QWidget* parent)
    : QWidget(parent),
      engine(engine_),
      time_scale(1.0),
      horizontalScroll(0),
      font("Arial", 9, QFont::Bold),
      bg_color("#32353b"),
      fg_color("#e0e6ef"),
      subline_color("#464a56"),
      tact_bg_color("#3c3f4f"),
      tact_line_color("#6f6fa6"),
      hover_color("#ff5858"),
      click_hint_color("#ff585880"),
      m_isHovered(false),
      m_hoverX(-1)
{
    setObjectName("MidiTactRuler");
    setFixedHeight(32);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void MidiTactRuler::setTimeScale(double time_scale) {
    this->time_scale = time_scale;
    update();
}

void MidiTactRuler::setHorizontalScroll(int val) {
    this->horizontalScroll = val;
    update();
}

void MidiTactRuler::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        NoteNagaProject *project = engine->getProject();
        int click_x = int(event->position().x()) + horizontalScroll;
        int tick = int(double(click_x) / (project->getPPQ() * time_scale) * project->getPPQ());
        emit positionSelected(tick);
    }
}

void MidiTactRuler::mouseMoveEvent(QMouseEvent* event) {
    m_hoverX = event->pos().x();
    update();
}

void MidiTactRuler::enterEvent(QEnterEvent* event) {
    Q_UNUSED(event);
    m_isHovered = true;
    update();
}

void MidiTactRuler::leaveEvent(QEvent* event) {
    Q_UNUSED(event);
    m_isHovered = false;
    m_hoverX = -1;
    update();
}

void MidiTactRuler::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    QRect r = rect();

    painter.fillRect(r, bg_color);
    painter.setFont(font);

    NoteNagaProject *project = engine->getProject();
    double ppq = project->getPPQ();
    double beat_px = ppq * time_scale;

    if (beat_px <= 0) return;

    int min_step_px = 60;
    int takt_step = 1;
    if (beat_px < min_step_px) {
        int pow2 = 1;
        while (beat_px * pow2 < min_step_px && pow2 <= 32) {
            pow2 *= 2;
        }
        takt_step = pow2;
    }
    if (takt_step <= 0) return;

    int sub_beats = 4;
    double sub_beat_px = beat_px / sub_beats;
    int width = r.width();

    // Správný výpočet: první takt na okně je zarovnán na takt_step
    int first_beat = int(std::floor(double(horizontalScroll) / (beat_px * takt_step)) * takt_step);
    if (first_beat < 0) first_beat = 0;

    for (int i = first_beat;; i += takt_step) {
        int x = int(i * beat_px - horizontalScroll);
        if (x > width) break;

        int next_x = int((i + takt_step) * beat_px - horizontalScroll);
        int bar_width = next_x - x;
        if (bar_width <= 0) break;
        if (next_x > width) {
            bar_width = width - x;
            if (bar_width <= 0) break;
        }

        // DULEZITE: Barva podle globalniho taktu, ne podle indexu v cyklu
        painter.fillRect(
            QRect(x, 0, bar_width, r.height()),
            ((i / takt_step) % 2 == 0) ? tact_bg_color : bg_color
        );
        painter.setPen(QPen(tact_line_color, 2));
        painter.drawLine(x, 0, x, r.height());
        if (-20 < x && x < width) {
            painter.setPen(fg_color);
            painter.drawText(x + 5, r.height() - 7, QString::number(i + 1));
        }
        if (takt_step == 1 && sub_beat_px > 15) {
            painter.setPen(QPen(subline_color, 1));
            for (int j = 1; j < sub_beats; ++j) {
                int subx = int(x + j * sub_beat_px);
                if (0 < subx && subx < width) {
                    painter.drawLine(subx, r.height() / 2, subx, r.height());
                }
            }
        }
        if (x + bar_width >= width) break;
    }
    
    // Draw hover line indicator - shows where playback position will be set on click
    if (m_isHovered && m_hoverX >= 0) {
        // Draw vertical line at hover position
        painter.setPen(QPen(hover_color, 2));
        painter.drawLine(m_hoverX, 0, m_hoverX, r.height());
        
        // Draw small triangle pointer at top
        QPolygon triangle;
        triangle << QPoint(m_hoverX - 5, 0)
                 << QPoint(m_hoverX + 5, 0)
                 << QPoint(m_hoverX, 8);
        painter.setBrush(hover_color);
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(triangle);
        
        // Draw semi-transparent hint area
        painter.fillRect(QRect(m_hoverX - 1, 0, 3, r.height()), click_hint_color);
    }
}