#include "midi_seq_progress_bar.h"

#include <QDebug>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtConcurrent>
#include <algorithm>
#include <chrono>
#include <cmath>

// --- Constructor ---
MidiSequenceProgressBar::MidiSequenceProgressBar(QWidget *parent)
    : QWidget(parent), midi_seq(nullptr), current_time(0.f), total_time(1.f),
      m_arrangementMode(false), waveform_resolution(400), m_computePending(false) {
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
    
    // Setup debounce timer for waveform refresh (300ms debounce)
    m_refreshDebounceTimer = new QTimer(this);
    m_refreshDebounceTimer->setSingleShot(true);
    m_refreshDebounceTimer->setInterval(300);
    connect(m_refreshDebounceTimer, &QTimer::timeout, this, &MidiSequenceProgressBar::computeWaveformAsync);
    
    // Setup future watcher for async waveform computation
    m_waveformWatcher = new QFutureWatcher<std::vector<float>>(this);
    connect(m_waveformWatcher, &QFutureWatcher<std::vector<float>>::finished,
            this, &MidiSequenceProgressBar::onWaveformComputeFinished);
}

MidiSequenceProgressBar::~MidiSequenceProgressBar()
{
    if (m_waveformWatcher->isRunning()) {
        m_waveformWatcher->cancel();
        m_waveformWatcher->waitForFinished();
    }
}

// --- Setters ---
void MidiSequenceProgressBar::setMidiSequence(NoteNagaMidiSeq *seq) {
    this->midi_seq = seq;
    if (!this->midi_seq) return;
    // Use initial/base tempo for total time calculation (not dynamic tempo)
    // This ensures waveform stability during playback with tempo changes
    double base_tempo_us = double(this->midi_seq->getTempo());  // Base tempo in microseconds per quarter
    double us_per_tick = base_tempo_us / double(this->midi_seq->getPPQ());
    this->total_time = midi_seq->getMaxTick() * us_per_tick / 1'000'000.0;
    refreshWaveform();
    update();
}

void MidiSequenceProgressBar::updateMaxTime() {
    // In arrangement mode, don't recalculate from midi_seq - use setTotalTime() instead
    if (m_arrangementMode) {
        update();
        return;
    }
    
    if (!midi_seq) return;
    // Use same base tempo calculation - don't recalculate waveform on every update
    double base_tempo_us = double(this->midi_seq->getTempo());
    double us_per_tick = base_tempo_us / double(this->midi_seq->getPPQ());
    double new_total_time = midi_seq->getMaxTick() * us_per_tick / 1'000'000.0;
    
    // Only refresh waveform if total time changed significantly (sequence modified)
    if (std::abs(new_total_time - this->total_time) > 0.1) {
        this->total_time = new_total_time;
        scheduleWaveformRefresh();  // Use async refresh
    }
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

void MidiSequenceProgressBar::setTotalTime(float seconds) {
    if (std::abs(total_time - seconds) > 0.01f) {
        total_time = std::max(0.01f, seconds);
        update();
    }
}

void MidiSequenceProgressBar::setArrangementMode(bool isArrangement) {
    if (m_arrangementMode != isArrangement) {
        m_arrangementMode = isArrangement;
        update();
    }
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

    // Calculate progress position
    float rel = (total_time > 0.01f) ? (current_time / total_time) : 0.f;
    float progress_x = bar_left + rel * bar_width;

    if (m_arrangementMode) {
        // --- Arrangement mode: simple progress bar without waveform ---
        if (bar_width > 10 && bar_height > 6) {
            // Draw filled progress area with gradient
            QLinearGradient gradient(bar_left, 0, bar_left + bar_width, 0);
            gradient.setColorAt(0, QColor("#3a5a3a"));
            gradient.setColorAt(1, QColor("#2a4a2a"));
            
            QRectF progressRect(bar_left, bar_top + 2, progress_x - bar_left, bar_height - 4);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#4a8a4a"));
            p.drawRect(progressRect);
            
            // Draw position indicator
            p.setPen(QPen(position_indicator_color, 2));
            p.drawLine(int(progress_x), bar_top, int(progress_x), bar_bottom);
            
            // Draw "ARRANGEMENT" label in center
            p.setPen(QColor("#888888"));
            QFont font = p.font();
            font.setPointSize(9);
            p.setFont(font);
            p.drawText(bar_rect, Qt::AlignCenter, tr("ARRANGEMENT"));
        }
    } else {
        // --- Sequence mode: Draw waveform ---
        if (bar_width > 10 && bar_height > 6 && !waveform.empty()) {
            int N = waveform_resolution;
            float xstep = float(bar_width) / N;
            // Draw dark waveform (background)
            for (int i = 0; i < N; ++i) {
                float val = waveform[i];
                float x0 = bar_left + i * xstep;
                float x1 = bar_left + (i + 1) * xstep;
                float y0 = bar_bottom - val * bar_height * 0.88f;
                QRectF rect(x0, y0, x1 - x0, bar_bottom - y0);
                p.setPen(Qt::NoPen);
                p.setBrush(waveform_fg);
                p.drawRect(rect);
            }
            // Overlay: colorize waveform up to progress position
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

// Public API - schedules a debounced waveform refresh
void MidiSequenceProgressBar::scheduleWaveformRefresh() {
    m_refreshDebounceTimer->start();  // Restart debounce timer
}

// Private - old sync implementation kept for setMidiSequence (immediate refresh needed)
void MidiSequenceProgressBar::refreshWaveform() {
    waveform = computeWaveformData();
    update();
}

// Async computation - runs in background thread
void MidiSequenceProgressBar::computeWaveformAsync() {
    if (!midi_seq || m_waveformWatcher->isRunning()) {
        // If already computing, mark as pending to re-run after completion
        if (m_waveformWatcher->isRunning()) {
            m_computePending.store(true);
        }
        return;
    }
    
    // Capture necessary data for thread-safe computation
    QFuture<std::vector<float>> future = QtConcurrent::run([this]() {
        return computeWaveformData();
    });
    
    m_waveformWatcher->setFuture(future);
}

// Called when async computation finishes
void MidiSequenceProgressBar::onWaveformComputeFinished() {
    if (m_waveformWatcher->isCanceled()) return;
    
    waveform = m_waveformWatcher->result();
    update();
    
    // If another refresh was requested while computing, restart
    if (m_computePending.exchange(false)) {
        computeWaveformAsync();
    }
}

// Thread-safe waveform computation (can run in background)
std::vector<float> MidiSequenceProgressBar::computeWaveformData() const {
    std::vector<float> result(waveform_resolution, 0.0f);

    if (!midi_seq) {
        return result;
    }
    if (midi_seq->getTracks().empty()) {
        return result;
    }

    std::vector<NoteNagaTrack *> tracks = midi_seq->getTracks();
    std::vector<const NN_Note_t *> notes;
    for (auto *t : tracks) {
        if (!t->isVisible() || t->isMuted() || t->isTempoTrack()) {
            continue;
        }
        for (const auto &n : t->getNotes()) {
            notes.push_back(&n);
        }
    }
    
    if (notes.empty()) {
        return result;
    }
    
    int N = waveform_resolution;
    std::vector<float> buckets(N, 0.0f);

    int ppq = midi_seq->getPPQ();
    int base_tempo = midi_seq->getTempo();  // Microseconds per quarter note
    if (ppq <= 0 || base_tempo <= 0) return result;
    
    float bpm = 60000000.0f / base_tempo;   // Convert to BPM
    float scale = 1.0f / 127.0f;
    
    // Calculate total duration in seconds from max tick
    int maxTick = midi_seq->getMaxTick();
    if (maxTick <= 0) maxTick = 1;
    float sequenceDuration = float(maxTick) / ppq * (60.0f / bpm);
    
    // Minimum duration for waveform visualization
    float effectiveDuration = std::max(sequenceDuration, 0.1f);

    for (const NN_Note_t *note : notes) {
        if (!note->start.has_value()) continue;
        
        // Calculate note position as a fraction of the total sequence
        float noteStartFraction = float(note->start.value()) / float(maxTick);
        float noteDurationTicks = note->length.has_value() ? float(note->length.value()) : (ppq / 4.0f);
        float noteEndFraction = float(note->start.value() + noteDurationTicks) / float(maxTick);
        
        // Clamp fractions
        noteStartFraction = std::clamp(noteStartFraction, 0.0f, 1.0f);
        noteEndFraction = std::clamp(noteEndFraction, 0.0f, 1.0f);
        
        float velocity = note->velocity.has_value() ? float(note->velocity.value()) : 90.f;

        int start_bucket = static_cast<int>(noteStartFraction * (N - 1));
        int end_bucket = static_cast<int>(noteEndFraction * (N - 1));
        
        // Ensure valid range
        start_bucket = std::clamp(start_bucket, 0, N - 1);
        end_bucket = std::clamp(end_bucket, 0, N - 1);
        
        // Ensure at least one bucket is filled
        if (end_bucket < start_bucket) end_bucket = start_bucket;
        
        // Fill buckets
        for (int b = start_bucket; b <= end_bucket; ++b) {
            buckets[b] += velocity * scale;
        }
        
        // For single notes, also add to neighbors for visibility
        if (start_bucket == end_bucket) {
            if (start_bucket > 0) buckets[start_bucket - 1] += velocity * scale * 0.3f;
            if (start_bucket < N - 1) buckets[start_bucket + 1] += velocity * scale * 0.3f;
        }
    }

    // Normalize
    float maxVal = *std::max_element(buckets.begin(), buckets.end());
    if (maxVal < 0.0001f) {
        // No data at all - return empty waveform
        return result;
    }

    for (int i = 0; i < N; ++i) {
        result[i] = std::clamp(buckets[i] / maxVal, 0.f, 1.f);
        // Apply slight curve for better visual contrast
        result[i] = std::pow(result[i], 0.7f);
    }
    
    return result;
}