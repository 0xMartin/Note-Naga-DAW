#include "midi_seq_progress_bar.h"

#include <QDebug>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <chrono>
#include <cmath>

// --- Constructor ---
MidiSequenceProgressBar::MidiSequenceProgressBar(QWidget *parent)
    : QWidget(parent), midi_seq(nullptr), current_time(0.f), total_time(1.f),
      waveform_resolution(400) {
    setMinimumWidth(300);
    setMinimumHeight(38);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    waveform.resize(waveform_resolution, 0.f);

    this->bar_bg = QColor("#30343a");
    this->box_bg = QColor("#2a2d32");
    this->outline_color = QColor("#21252f");
    this->waveform_fg = QColor("#426289");
    this->waveform_fg_active = QColor("#5aa7ff");
    this->position_indicator_color = QColor("#c04a4a");

    waveform_img = QImage();
    waveform_img_width = 0;
    waveform_img_height = 0;
}

// --- Setters ---
void MidiSequenceProgressBar::setMidiSequence(NoteNagaMidiSeq *seq) {
    this->midi_seq = seq;
    if (!this->midi_seq) return;
    double us_per_tick =
        double(this->midi_seq->getTempo()) / double(this->midi_seq->getPPQ());
    this->total_time = midi_seq->getMaxTick() * us_per_tick / 1'000'000.0;
    refreshWaveform();
    update();
}

void MidiSequenceProgressBar::updateMaxTime() {
    double us_per_tick =
        double(this->midi_seq->getTempo()) / double(this->midi_seq->getPPQ());
    this->total_time = midi_seq->getMaxTick() * us_per_tick / 1'000'000.0;
    refreshWaveform();
    update();
}

void MidiSequenceProgressBar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

void MidiSequenceProgressBar::setCurrentTime(float seconds) {
    //if (std::abs(current_time - seconds) < 0.25) return;
    current_time = std::clamp(seconds, 0.f, total_time);
    update();
}

QString MidiSequenceProgressBar::formatTime(float seconds) const {
    int s = int(seconds + 0.5);
    int m = s / 60;
    int sec = s % 60;
    return QString("%1:%2").arg(m).arg(sec, 2, 10, QChar('0'));
}

// --- Paint ---
void MidiSequenceProgressBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Geometry
    int w = width(), h = height();
    int label_padding = 11; // zvětšeno o 4 px
    int label_width = 34;
    int box_pad = 2; // padding uvnitř progressbar boxu
    int bar_left = label_width + label_padding;
    int bar_right = w - (label_width + label_padding);
    int bar_width = bar_right - bar_left;
    int bar_top = box_pad;
    int bar_bottom = h - box_pad - 1;
    int bar_height = bar_bottom - bar_top + 1;
    int corner_radius = 8;

    QRect bar_rect(bar_left, bar_top - box_pad, bar_width, bar_height + 2 * box_pad);
    QRect left_label_rect(0, bar_top - box_pad, bar_left, bar_height + 2 * box_pad);
    QRect right_label_rect(bar_right, bar_top - box_pad, w - bar_right,
                           bar_height + 2 * box_pad);

    // Draw central bar background (no rounding on ends)
    p.setPen(Qt::NoPen);
    p.setBrush(bar_bg);
    p.drawRect(bar_rect);

    // --- Draw waveform: sharp, no gaps ---
    if (bar_width > 10 && bar_height > 6 && !waveform.empty()) {
        int N = waveform_resolution;
        float xstep = float(bar_width) / N;
        // Draw dark waveform (background)
        for (int i = 0; i < N; ++i) {
            float val = waveform[i];
            // Pozor: x není zaokrouhlováno na int, ale je to float
            float x0 = bar_left + i * xstep;
            float x1 = bar_left + (i + 1) * xstep;
            float y0 = bar_bottom - val * bar_height * 0.88f;
            QRectF rect(x0, y0, x1 - x0, bar_bottom - y0);
            p.setPen(Qt::NoPen);
            p.setBrush(waveform_fg);
            p.drawRect(rect);
        }
        // Overlay: colorize waveform up to progress position
        float rel = (total_time > 0.1f) ? (current_time / total_time) : 0.f;
        float progress_x = bar_left + rel * bar_width;
        for (int i = 0; i < N; ++i) {
            float val = waveform[i];
            float x0 = bar_left + i * xstep;
            float x1 = bar_left + (i + 1) * xstep;
            if (x0 >= progress_x) break;
            float y0 = bar_bottom - val * bar_height * 0.88f;
            QRectF rect(x0, y0, std::min(x1, progress_x) - x0, bar_bottom - y0);
            p.setPen(Qt::NoPen);
            p.setBrush(waveform_fg_active);
            p.drawRect(rect);
        }
        // --- Draw red position indicator (thin vertical bar) ---
        p.setPen(QPen(position_indicator_color, 1.5));
        p.drawLine(int(progress_x), bar_top, int(progress_x), bar_bottom);
    }

    // --- Draw outline ---
    p.setPen(QPen(outline_color, 1));
    p.setBrush(bar_bg);
    // 1. Outline top and bottom of central bar
    p.drawLine(bar_rect.left(), bar_rect.top(), bar_rect.right(), bar_rect.top());
    p.drawLine(bar_rect.left(), bar_rect.bottom(), bar_rect.right(), bar_rect.bottom());
    // 2. Outline left box (only left, top, bottom curves)
    p.setBrush(box_bg);
    {
        QPainterPath path;
        path.moveTo(left_label_rect.right(), left_label_rect.top());
        path.lineTo(left_label_rect.left() + corner_radius, left_label_rect.top());
        path.arcTo(left_label_rect.left(), left_label_rect.top(), corner_radius * 2,
                   corner_radius * 2, 90, 90);
        path.lineTo(left_label_rect.left(), left_label_rect.bottom() - corner_radius);
        path.arcTo(left_label_rect.left(), left_label_rect.bottom() - corner_radius * 2,
                   corner_radius * 2, corner_radius * 2, 180, 90);
        path.lineTo(left_label_rect.right(), left_label_rect.bottom());
        p.drawPath(path);
    }
    // 3. Outline right box (only right, top, bottom curves)
    {
        QPainterPath path;
        path.moveTo(right_label_rect.left(), right_label_rect.top());
        path.lineTo(right_label_rect.right() - corner_radius, right_label_rect.top());
        path.arcTo(right_label_rect.right() - corner_radius * 2, right_label_rect.top(),
                   corner_radius * 2, corner_radius * 2, 90, -90);
        path.lineTo(right_label_rect.right(), right_label_rect.bottom() - corner_radius);
        path.arcTo(right_label_rect.right() - corner_radius * 2,
                   right_label_rect.bottom() - corner_radius * 2, corner_radius * 2,
                   corner_radius * 2, 0, -90);
        path.lineTo(right_label_rect.left(), right_label_rect.bottom());
        p.drawPath(path);
    }

    // --- Draw time labels ---
    QFont font = this->font();
    font.setPointSize(13);
    font.setBold(true);
    p.setFont(font);
    p.setPen(QColor("#eee"));
    QString left_label = formatTime(current_time);
    QString right_label = formatTime(total_time);

    int label_h = p.fontMetrics().height();
    int label_y = bar_top + (bar_height + label_h) / 2 - 2;

    QRect left_text_rect(left_label_rect.left(), label_y - label_h + 2,
                         left_label_rect.width() - 6, label_h + 4);
    p.drawText(left_text_rect, Qt::AlignRight | Qt::AlignVCenter, left_label);

    QRect right_text_rect(right_label_rect.left() + 6, label_y - label_h + 2,
                          right_label_rect.width() - 6, label_h + 4);
    p.drawText(right_text_rect, Qt::AlignLeft | Qt::AlignVCenter, right_label);

    p.end();
}

// --- Mouse seeking ---
float MidiSequenceProgressBar::mapMouseEventToTime(QMouseEvent *event) const {
    int w = width();
    int label_width = 34;
    int label_padding = 11;
    int box_pad = 2;
    int bar_left = label_width + label_padding;
    int bar_right = w - (label_width + label_padding);
    int bar_width = bar_right - bar_left;
    int bar_top = box_pad;
    int bar_bottom = height() - box_pad - 1;

    if (event->position().x() < bar_left || event->position().y() > bar_bottom) return -1.f;
    if (event->position().x() < bar_left || event->position().x() > bar_right) return -1.f;

    float rel = float(event->position().x() - bar_left) / bar_width;
    return rel * total_time;
}

void MidiSequenceProgressBar::mousePressEvent(QMouseEvent *event) {
    float sec = mapMouseEventToTime(event);
    if (sec >= 0.f)
        emit positionPressed(sec);
}

void MidiSequenceProgressBar::mouseMoveEvent(QMouseEvent *event) {
    float sec = mapMouseEventToTime(event);
    if (sec >= 0.f)
        emit positionDragged(sec);
}

void MidiSequenceProgressBar::mouseReleaseEvent(QMouseEvent *event) {
    float sec = mapMouseEventToTime(event);
    if (sec >= 0.f)
        emit positionReleased(sec);
}

// --- Waveform: Precompute from MIDI sequence ---
void MidiSequenceProgressBar::refreshWaveform() {
    std::fill(waveform.begin(), waveform.end(), 0.0f);

    if (!midi_seq || midi_seq->getTracks().empty() || total_time < 0.1f) return;

    std::vector<NoteNagaTrack *> tracks = midi_seq->getTracks();
    std::vector<const NN_Note_t *> notes;
    for (auto *t : tracks) {
        if (!t->isVisible() || t->isMuted()) continue;
        for (const auto &n : t->getNotes()) {
            notes.push_back(&n);
        }
    }
    int N = waveform_resolution;
    float bucket_dur = total_time / N;
    std::vector<float> buckets(N, 0.0f);

    int ppq = midi_seq->getPPQ();
    int tempo = midi_seq->getTempo();
    float scale = 1.0f / 127.0f;

    for (const NN_Note_t *note : notes) {
        if (!note->start.has_value()) continue;
        float start_sec =
            float(note->start.value()) / ppq * (60.0f / (60000000.0f / tempo));
        float dur_sec =
            note->length.has_value()
                ? (float(note->length.value()) / ppq * (60.0f / (60000000.0f / tempo)))
                : 0.1f;
        float velocity =
            note->velocity.has_value() ? float(note->velocity.value()) : 90.f;

        int start_bucket = std::max(0, std::min(N - 1, int(start_sec / bucket_dur)));
        int end_bucket =
            std::max(0, std::min(N - 1, int((start_sec + dur_sec) / bucket_dur)));
        for (int b = start_bucket; b <= end_bucket; ++b) {
            buckets[b] += velocity * scale;
        }
    }

    std::vector<float> sorted_buckets = buckets;
    std::sort(sorted_buckets.begin(), sorted_buckets.end());

    float percentile = 0.98;
    int idx = std::min(N-1, int(N * percentile));
    float norm_value = sorted_buckets[idx];
    if (norm_value < 1.0f) norm_value = 1.0f; 

    for (int i = 0; i < N; ++i) {
        waveform[i] = std::clamp(buckets[i] / norm_value, 0.f, 1.f);
        waveform[i] = std::pow(waveform[i], 0.7f);
}
}