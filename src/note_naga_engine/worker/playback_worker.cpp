#include "playback_worker.h"
#include <QDebug>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <chrono>
#include <thread>

// --- PlaybackWorker ---

PlaybackWorker::PlaybackWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval_ms,
                               QObject *parent)
    : QObject(parent),
      project(project),
      mixer(mixer),
      timer_interval(timer_interval_ms / 1000.0),
      playing(false),
      thread(nullptr),
      worker(nullptr),
      should_stop(false)
{
}

void PlaybackWorker::recalculate_worker_tempo()
{
    if (!worker)
    {
        qDebug() << "PlaybackWorker: Worker is not running, unable to recalculate tempo.";
        return;
    }
    worker->recalculate_tempo();
}

bool PlaybackWorker::play()
{
    if (playing)
    {
        qDebug() << "PlaybackWorker: Already playing.";
        return false;
    }
    if (!project)
    {
        qDebug() << "PlaybackWorker: No project data available.";
        return false;
    }
    qDebug() << "PlaybackWorker: Starting playback.";

    thread = new QThread;
    worker = new PlaybackThreadWorker(project, mixer, timer_interval);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &PlaybackThreadWorker::run);
    connect(worker, &PlaybackThreadWorker::finished_signal, thread, &QThread::quit);
    connect(worker, &PlaybackThreadWorker::finished_signal, this, &PlaybackWorker::cleanup_thread);
    connect(worker, &PlaybackThreadWorker::on_position_changed_signal, this, &PlaybackWorker::on_position_changed_signal);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
    playing = true;
    NN_QT_EMIT(playing_state_changed_signal(playing));
    return true;
}

bool PlaybackWorker::stop()
{
    if (!playing)
    {
        qDebug() << "PlaybackWorker: Not currently playing.";
        return false;
    }
    qDebug() << "PlaybackWorker: Stopping playback.";

    if (worker)
        worker->stop();
    if (thread)
    {
        thread->quit();
        thread->wait();
    }
    playing = false;
    NN_QT_EMIT(playing_state_changed_signal(playing));
    return true;
}

void PlaybackWorker::cleanup_thread()
{
    if (worker)
    {
        worker->deleteLater();
        worker = nullptr;
    }
    if (thread)
    {
        thread->wait();
        thread->deleteLater();
        thread = nullptr;
    }
    playing = false;
    NN_QT_EMIT(playing_state_changed_signal(playing));
}

// --- PlaybackThreadWorker ---

PlaybackThreadWorker::PlaybackThreadWorker(NoteNagaProject *project, NoteNagaMixer *mixer, double timer_interval)
    : QObject(nullptr),
      project(project),
      mixer(mixer),
      timer_interval(timer_interval),
      ms_per_tick(1.0),
      should_stop(false)
{
}

void PlaybackThreadWorker::recalculate_tempo()
{
    int current_tick = this->project->get_current_tick();
    double us_per_tick = static_cast<double>(this->project->get_tempo()) / this->project->get_ppq();
    ms_per_tick = us_per_tick / 1000.0;
    start_time_point = std::chrono::high_resolution_clock::now();
    start_tick_at_start = current_tick;
}

void PlaybackThreadWorker::run()
{
    qDebug() << "PlaybackThreadWorker: run() start";

    // get active sequence from project data
    NoteNagaMIDISeq *active_sequence = this->project->get_active_sequence();
    if (!active_sequence)
    {
        qDebug() << "No active sequence found, cannot run playback worker.";
        return;
    }

    int current_tick = this->project->get_current_tick();
    recalculate_tempo();

    using clock = std::chrono::high_resolution_clock;

    while (!should_stop)
    {
        auto now = clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(now - start_time_point).count();
        int target_tick = start_tick_at_start + static_cast<int>(elapsed_ms / ms_per_tick);
        int tick_advance = std::max(1, target_tick - current_tick);
        int last_tick = current_tick;
        current_tick += tick_advance;
        this->project->set_current_tick(current_tick);

        // Stop playback on reaching max tick
        if (current_tick >= active_sequence->get_max_tick())
        {
            current_tick = active_sequence->get_max_tick();
            should_stop = true;
        }

        // check if some track is soloed (store id of the soloed track)
        if (active_sequence->get_solo_track())
        {
            // send all note on / off events to mixer
            auto track = active_sequence->get_solo_track();
            if (track)
            {
                for (const auto &note : track->get_notes())
                {
                    if (note.start.has_value() && note.length.has_value())
                    {
                        if (last_tick < note.start.value() && note.start.value() <= current_tick)
                            mixer->note_play(note);
                        if (last_tick < note.start.value() + note.length.value() && note.start.value() + note.length.value() <= current_tick)
                            mixer->note_stop(note);
                    }
                }
            }
        }
        else
        {
            // Mixer note events
            for (const auto &track : active_sequence->get_tracks())
            {
                // Skip muted tracks
                if (track->is_muted())
                    continue;
                // send all note on / off events to mixer
                for (const auto &note : track->get_notes())
                {
                    if (note.start.has_value() && note.length.has_value())
                    {
                        if (last_tick < note.start.value() && note.start.value() <= current_tick)
                            mixer->note_play(note);
                        if (last_tick < note.start.value() + note.length.value() && note.start.value() + note.length.value() <= current_tick)
                            mixer->note_stop(note);
                    }
                }
            }
        }

        // Postion changed signal
        NN_QT_EMIT(on_position_changed_signal(current_tick));

        std::this_thread::sleep_for(std::chrono::duration<double>(timer_interval));
    }

    qDebug() << "PlaybackThreadWorker: finished";

    NN_QT_EMIT(finished_signal());
}

void PlaybackThreadWorker::stop()
{
    should_stop = true;
}