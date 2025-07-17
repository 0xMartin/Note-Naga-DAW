#include "midi_keyboard_ruler.h"

#include <QPainter>
#include <QLinearGradient>
#include <QBrush>
#include <QRect>
#include <QTimer>
#include <QEvent>
#include <random>
#include "../core/shared.h"

MidiKeyboardRuler::MidiKeyboardRuler(AppContext *ctx, int min_note_, int max_note_, int viewer_row_height_, QWidget *parent)
    : QWidget(parent),
      ctx(ctx),
      min_note(min_note_),
      max_note(max_note_),
      viewer_row_height(viewer_row_height_),
      verticalScroll(0),
      pressed_note(-1),
      hovered_note(-1),
      font("Arial", 10),
      c_key_font("Arial", 10, QFont::Bold),
      bg_color("#262a33"),
      white_key_color("#e7edf5"),
      black_key_color("#1e2537"),
      white_key_line_color("#b0b8c8"),
      black_key_line_color("#242a30"),
      hover_color("#7e93be"),
      press_color("#4f76c4"),
      c_key_label_color("#1c305e")
{
    connect(ctx, &AppContext::playing_note_signal, this, &MidiKeyboardRuler::on_play_note);
    setObjectName("MidiKeyboardRuler");
    setMinimumWidth(60);
    setMouseTracking(true);
}

QSize MidiKeyboardRuler::sizeHint() const
{
    return QSize(minimumWidth(), 10);
}
QSize MidiKeyboardRuler::minimumSizeHint() const
{
    return sizeHint();
}

bool MidiKeyboardRuler::is_black_key(int midi_note)
{
    int mod = midi_note % 12;
    for (int i = 0; i < 5; ++i)
    {
        if (mod == BLACK_ORDER[i])
            return true;
    }
    return false;
}
bool MidiKeyboardRuler::is_white_key(int midi_note)
{
    int mod = midi_note % 12;
    for (int i = 0; i < 7; ++i)
    {
        if (mod == WHITE_ORDER[i])
            return true;
    }
    return false;
}

std::vector<int> MidiKeyboardRuler::white_keys() const
{
    std::vector<int> result;
    for (int n = max_note; n >= min_note; --n)
        if (is_white_key(n))
            result.push_back(n);
    return result;
}
std::vector<int> MidiKeyboardRuler::black_keys() const
{
    std::vector<int> result;
    for (int n = max_note; n >= min_note; --n)
        if (is_black_key(n))
            result.push_back(n);
    return result;
}

std::optional<int> MidiKeyboardRuler::note_at_pos(const QPoint &pos) const
{
    int key_w = width();
    auto white_keys_vec = white_keys();
    // Compute y positions of each white key
    double y_cursor = 0.0;
    std::vector<std::tuple<int, double, double, double>> key_positions;
    for (size_t i = 0; i < white_keys_vec.size(); ++i)
    {
        int n = white_keys_vec[i];
        double size_factor = (i == 0) ? 1.5 : WHITE_HEIGHT[index_in_octave(n)];
        double white_key_height = size_factor * viewer_row_height;
        double y = y_cursor + verticalScroll;
        key_positions.emplace_back(n, y, y + white_key_height, size_factor);
        y_cursor += white_key_height;
    }

    // Black keys
    for (int n = max_note; n >= min_note; --n)
    {
        if (!is_black_key(n))
            continue;
        int idx = -1;
        for (size_t i = 0; i < white_keys_vec.size() - 1; ++i)
        {
            if (white_keys_vec[i] > n && n > white_keys_vec[i + 1])
            {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx != -1)
        {
            double y = std::get<1>(key_positions[idx]) + int(viewer_row_height * (std::get<3>(key_positions[idx]) - 0.5));
            int black_h = viewer_row_height;
            int black_w = int(key_w * 0.65);
            int black_x = 0;
            int black_y = int(y);
            QRect rect(black_x, black_y, black_w, black_h);
            if (rect.contains(pos))
                return n;
        }
    }

    // White keys
    int y = pos.y();
    for (const auto &kp : key_positions)
    {
        int n = std::get<0>(kp);
        double y0 = std::get<1>(kp);
        double y1 = std::get<2>(kp);
        if (y0 <= y && y < y1)
            return n;
    }
    return std::nullopt;
}

void MidiKeyboardRuler::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(rect(), bg_color);
    painter.setFont(font);
    int key_w = width();
    auto white_keys_vec = white_keys();

    int visible_y0 = 0;
    int visible_y1 = height();

    // WHITE KEYS
    double y_cursor = 0.0;
    std::vector<std::tuple<int, double, double, double>> key_positions;
    for (size_t i = 0; i < white_keys_vec.size(); ++i)
    {
        int n = white_keys_vec[i];
        double size_factor = (i == 0) ? 1.5 : WHITE_HEIGHT[index_in_octave(n)];
        double white_key_height = size_factor * viewer_row_height;
        double y = y_cursor + verticalScroll;
        if (y + white_key_height < visible_y0 - white_key_height * 2 || y > visible_y1 + white_key_height * 2)
        {
            y_cursor += white_key_height;
            continue;
        }
        QRect rect(0, int(y), key_w, int(white_key_height));
        QColor fill_color = key_highlights.contains(n) ? key_highlights[n] : QColor();
        if (!fill_color.isValid())
        {
            if (n == pressed_note)
                fill_color = press_color;
            else if (n == hovered_note)
                fill_color = hover_color;
            else
                fill_color = white_key_color;
        }
        painter.fillRect(rect, fill_color);
        painter.setPen(white_key_line_color);
        painter.drawRect(rect);
        if (n % 12 == 0)
        {
            QString note_name_str = note_name(n);
            painter.setPen(c_key_label_color);
            painter.setFont(c_key_font);
            QRect text_rect(0, int(y - 2), key_w - 2, int(white_key_height));
            painter.drawText(text_rect, Qt::AlignRight | Qt::AlignBottom, note_name_str);
            painter.setFont(font);
        }
        key_positions.emplace_back(n, y, y + white_key_height, size_factor);
        y_cursor += white_key_height;
    }

    // BLACK KEYS
    for (int n = max_note; n >= min_note; --n)
    {
        if (!is_black_key(n))
            continue;
        int idx = -1;
        for (size_t i = 0; i < key_positions.size() - 1; ++i)
        {
            if (std::get<0>(key_positions[i]) > n && n > std::get<0>(key_positions[i + 1]))
            {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx != -1)
        {
            double y = std::get<1>(key_positions[idx]) + int(viewer_row_height * (std::get<3>(key_positions[idx]) - 0.5));
            int black_h = viewer_row_height;
            int black_w = int(key_w * 0.65);
            int black_x = 0;
            int black_y = int(y);
            if (black_y + black_h < visible_y0 || black_y > visible_y1)
                continue;
            QRect rect(black_x, black_y, black_w, black_h);
            QLinearGradient grad(black_x, black_y, black_x + black_w, black_y);
            QColor fill_color = key_highlights.contains(n) ? key_highlights[n] : QColor();
            if (fill_color.isValid())
            {
                painter.fillRect(rect, fill_color);
            }
            else if (n == pressed_note)
            {
                grad.setColorAt(1, press_color);
                grad.setColorAt(0, black_key_color);
                painter.fillRect(rect, QBrush(grad));
            }
            else if (n == hovered_note)
            {
                grad.setColorAt(1, hover_color);
                grad.setColorAt(0, black_key_color);
                painter.fillRect(rect, QBrush(grad));
            }
            else
            {
                grad.setColorAt(0, black_key_color.lighter(106));
                grad.setColorAt(1, black_key_color);
                painter.fillRect(rect, QBrush(grad));
            }
            painter.setPen(black_key_line_color);
            painter.drawRect(rect);
        }
    }
}

void MidiKeyboardRuler::mouseMoveEvent(QMouseEvent *event)
{
    auto note = note_at_pos(event->pos());
    int nval = note.has_value() ? note.value() : -1;
    if (nval != hovered_note)
    {
        hovered_note = nval;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void MidiKeyboardRuler::leaveEvent(QEvent *event)
{
    hovered_note = -1;
    update();
    QWidget::leaveEvent(event);
}

void MidiKeyboardRuler::mousePressEvent(QMouseEvent *event)
{
    auto note = note_at_pos(event->pos());
    int nval = note.has_value() ? note.value() : -1;
    if (nval != -1)
    {
        pressed_note = nval;
        update();
        // random velocity 44 - 84
        int velocity = 44 + rand() % 41;
        MidiNote midi_note;
        midi_note.note = nval;
        midi_note.velocity = velocity;
        emit play_note_signal(midi_note);
    }
    QWidget::mousePressEvent(event);
}

void MidiKeyboardRuler::mouseReleaseEvent(QMouseEvent *event)
{
    if (pressed_note != -1)
    {
        int velocity = 44 + rand() % 41;
        MidiNote midi_note;
        midi_note.note = pressed_note;
        midi_note.velocity = velocity;
        emit stop_note_signal(midi_note);
        pressed_note = -1;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void MidiKeyboardRuler::set_vertical_scroll_slot(float v, float row_height)
{
    viewer_row_height = int(row_height);
    verticalScroll = -int(v);
    update();
}

void MidiKeyboardRuler::on_play_note(const MidiNote &note)
{
    int timeout = note_time_ms(note, this->ctx->ppq, this->ctx->tempo);
    std::shared_ptr<Track> track = this->ctx->get_track_by_id(note.track.value_or(0));
    highlight_key(note.note, track->color, timeout);
}

void MidiKeyboardRuler::highlight_key(int note, const QColor &color, int timeout)
{
    key_highlights[note] = color;

    // Správa timerů
    if (highlight_timers.contains(note))
    {
        highlight_timers[note]->stop();
        highlight_timers[note]->deleteLater();
        highlight_timers.remove(note);
    }
    if (timeout > 0)
    {
        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, note](){ 
            _remove_highlight(note); 
        });
        timer->start(timeout);
        highlight_timers[note] = timer;
    }

    update();
}

void MidiKeyboardRuler::_remove_highlight(int note)
{
    if (key_highlights.contains(note))
        key_highlights.remove(note);
    if (highlight_timers.contains(note))
    {
        highlight_timers[note]->deleteLater();
        highlight_timers.remove(note);
    }
    update();
}

void MidiKeyboardRuler::clear_highlights_slot()
{
    clear_highlights();
}

void MidiKeyboardRuler::clear_highlights()
{
    key_highlights.clear();
    for (auto timer : highlight_timers)
    {
        timer->stop();
        timer->deleteLater();
    }
    highlight_timers.clear();
    update();
}