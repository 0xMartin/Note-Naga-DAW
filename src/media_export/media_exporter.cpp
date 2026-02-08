#include "media_exporter.h"

#include "media_renderer.h"
#include <note_naga_engine/note_naga_engine.h>
#include <note_naga_engine/nn_utils.h>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <fstream>
#include <QProcess>
#include <QFileInfo>
#include <QtConcurrent>
#include <QFuture>
#include <QTextStream> 
#include <numeric>
#include <memory>

// This guard class ensures that the engine is in manual audio rendering mode
// during the audio export and switches back automatically when done.
class ManualModeGuard
{
public:
    ManualModeGuard(NoteNagaEngine *engine, NoteNagaMidiSeq *sequence) 
        : m_engine(engine), m_sequence(sequence), m_arrangement(nullptr)
    {
        if (m_engine)
        {
            // Enter manual mode on all track synths
            if (m_sequence) {
                for (auto *track : m_sequence->getTracks()) {
                    if (track && track->getSynth()) {
                        track->getSynth()->enterManualMode();
                    }
                }
            }
            m_engine->getAudioWorker()->mute();
        }
    }
    
    ManualModeGuard(NoteNagaEngine *engine, NoteNagaArrangement *arrangement, NoteNagaRuntimeData *runtimeData)
        : m_engine(engine), m_sequence(nullptr), m_arrangement(arrangement)
    {
        if (m_engine && runtimeData)
        {
            // Enter manual mode on all synths from all sequences in clips
            for (NoteNagaMidiSeq* seq : runtimeData->getSequences()) {
                if (!seq) continue;
                for (auto *track : seq->getTracks()) {
                    if (track && track->getSynth()) {
                        track->getSynth()->enterManualMode();
                    }
                }
            }
            m_engine->getAudioWorker()->mute();
        }
    }
    
    ~ManualModeGuard()
    {
        if (m_engine)
        {
            NoteNagaRuntimeData* runtimeData = m_engine->getRuntimeData();
            // Exit manual mode on all track synths
            if (m_sequence) {
                for (auto *track : m_sequence->getTracks()) {
                    if (track && track->getSynth()) {
                        track->getSynth()->exitManualMode();
                    }
                }
            } else if (m_arrangement && runtimeData) {
                for (NoteNagaMidiSeq* seq : runtimeData->getSequences()) {
                    if (!seq) continue;
                    for (auto *track : seq->getTracks()) {
                        if (track && track->getSynth()) {
                            track->getSynth()->exitManualMode();
                        }
                    }
                }
            }
            m_engine->getAudioWorker()->unmute();
        }
    }

private:
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    NoteNagaArrangement *m_arrangement;
};


MediaExporter::MediaExporter(NoteNagaMidiSeq *sequence, QString outputPath,
                             QSize resolution, int fps, NoteNagaEngine *engine,
                             double secondsVisible,
                             const MediaRenderer::RenderSettings &settings,
                             ExportMode exportMode, 
                             const QString& audioFormat, 
                             int audioBitrate, 
                             QObject *parent)
    : QObject(parent), m_sequence(sequence), m_arrangement(nullptr), 
      m_sourceMode(SingleSequence), m_outputPath(outputPath),
      m_resolution(resolution), m_fps(fps), m_engine(engine),
      m_secondsVisible(secondsVisible), m_settings(settings), 
      m_exportMode(exportMode), 
      m_audioFormat(audioFormat), 
      m_audioBitrate(audioBitrate), 
      m_framesRendered(0), m_totalFrames(0)
{
    connect(&m_audioWatcher, &QFutureWatcher<bool>::finished, this, &MediaExporter::onTaskFinished);
    connect(&m_videoWatcher, &QFutureWatcher<bool>::finished, this, &MediaExporter::onTaskFinished);
}

MediaExporter::MediaExporter(NoteNagaArrangement *arrangement, QString outputPath,
                             QSize resolution, int fps, NoteNagaEngine *engine,
                             double secondsVisible,
                             const MediaRenderer::RenderSettings &settings,
                             ExportMode exportMode, 
                             const QString& audioFormat, 
                             int audioBitrate, 
                             QObject *parent)
    : QObject(parent), m_sequence(nullptr), m_arrangement(arrangement),
      m_sourceMode(Arrangement), m_outputPath(outputPath),
      m_resolution(resolution), m_fps(fps), m_engine(engine),
      m_secondsVisible(secondsVisible), m_settings(settings), 
      m_exportMode(exportMode), 
      m_audioFormat(audioFormat), 
      m_audioBitrate(audioBitrate), 
      m_framesRendered(0), m_totalFrames(0)
{
    connect(&m_audioWatcher, &QFutureWatcher<bool>::finished, this, &MediaExporter::onTaskFinished);
    connect(&m_videoWatcher, &QFutureWatcher<bool>::finished, this, &MediaExporter::onTaskFinished);
}

MediaExporter::~MediaExporter()
{
    cleanup();
}

void MediaExporter::doExport()
{
    m_tempAudioPath = m_outputPath + ".tmp.wav";

    if (m_exportMode == Video)
    {
        // This will be the final temporary video after joining batches
        m_tempVideoPath = m_outputPath + ".tmp.video.mp4"; 

        emit statusTextChanged(tr("Rendering in progress..."));

        m_finishedTaskCount = 0;

        QFuture<bool> audioFuture = QtConcurrent::run([this]()
                                                      { return this->exportAudio(m_tempAudioPath); });

        // Run the new batched method for video
        QFuture<bool> videoFuture = QtConcurrent::run([this]()
                                                      { return this->exportVideoBatched(m_tempVideoPath); });

        m_audioWatcher.setFuture(audioFuture);
        m_videoWatcher.setFuture(videoFuture);
    }
    else // AudioOnly
    {
        emit statusTextChanged(tr("Rendering audio..."));
        
        // Only export audio
        QFuture<bool> audioFuture = QtConcurrent::run([this]()
                                                      { return this->exportAudio(m_tempAudioPath); });
        
        m_audioWatcher.setFuture(audioFuture);
        // Video watcher will not be started
    }
}

void MediaExporter::onTaskFinished()
{
    if (m_exportMode == Video)
    {
        // Video mode: Waiting for 2 tasks (audio + video)
        if (m_finishedTaskCount.fetchAndAddOrdered(1) + 1 != 2)
        {
            return;
        }

        bool audioSuccess = m_audioWatcher.future().result();
        bool videoSuccess = m_videoWatcher.future().result();

        if (!audioSuccess)
        {
            emit error(tr("Failed to render audio."));
            cleanup();
            emit finished();
            return;
        }

        if (!videoSuccess)
        {
            emit error(tr("Failed to render video."));
            cleanup();
            emit finished();
            return;
        }

        emit statusTextChanged(tr("Combining files (muxing)..."));

        if (!combineAudioVideo(m_tempVideoPath, m_tempAudioPath, m_outputPath))
        {
            emit error(tr("Failed to combine video and audio. Is FFmpeg installed and in the system PATH?"));
        }
    }
    else // AudioOnly
    {
        // Waiting only for audio task
        bool audioSuccess = m_audioWatcher.future().result();
        if (!audioSuccess)
        {
            emit error(tr("Failed to render audio."));
            cleanup();
            emit finished();
            return;
        }

        emit statusTextChanged(tr("Converting audio format..."));
        
        // Transcode to desired format
        if (!transcodeAudio(m_tempAudioPath, m_outputPath, m_audioFormat, m_audioBitrate))
        {
            emit error(tr("Failed to convert audio. Is FFmpeg installed and in the system PATH?"));
        }
    }

    // Common cleanup and finishing
    cleanup();
    emit finished();
}

void MediaExporter::cleanup()
{
    // Delete temporary files for audio
    if (!m_tempAudioPath.isEmpty())
        QFile::remove(m_tempAudioPath);

    // Temporary data for video are deleted only in Video mode
    if (m_exportMode == Video)
    {
        if (!m_tempVideoPath.isEmpty())
            QFile::remove(m_tempVideoPath);
            
        // We must also delete temporary batches
        QDir dir = QFileInfo(m_outputPath).dir();
        QString baseName = QFileInfo(m_outputPath).fileName();
        QStringList filters;
        filters << baseName + ".tmp.batch.*.mp4";
        for(const QString& tmpFile : dir.entryList(filters, QDir::Files)) {
            QFile::remove(dir.filePath(tmpFile));
        }
        QFile::remove(dir.filePath("filelist.txt"));
    }
}

bool MediaExporter::exportAudio(const QString &outputPath)
{
    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    
    const int sampleRate = 44100;
    const int numChannels = 2;
    
    if (m_sourceMode == Arrangement) {
        return exportAudioArrangement(outputPath);
    }
    
    // Single Sequence mode
    ManualModeGuard manualMode(this->m_engine, m_sequence);

    NoteNagaMidiSeq *activeSequence = m_sequence ? m_sequence : project->getActiveSequence();
    if (!activeSequence) return false;
    
    const double totalDuration = nn_ticks_to_seconds(activeSequence->getMaxTick(), project->getPPQ(), project->getTempo()) + 2.0;
    const int totalSamples = static_cast<int>(totalDuration * sampleRate);

    std::vector<float> audioBuffer(totalSamples * numChannels, 0.0f);

    struct MidiEvent
    {
        int tick;
        NN_Note_t note;
        bool isNoteOn;
    };
    std::vector<MidiEvent> allEvents;
    for (auto *track : activeSequence->getTracks())
    {
        if (track->isMuted() || (activeSequence->getSoloTrack() && activeSequence->getSoloTrack() != track))
            continue;
        for (const auto &note : track->getNotes())
        {
            if (note.start.has_value() && note.length.has_value())
            {
                allEvents.push_back({note.start.value(), note, true});
                allEvents.push_back({note.start.value() + note.length.value(), note, false});
            }
        }
    }
    std::sort(allEvents.begin(), allEvents.end(), [](const MidiEvent &a, const MidiEvent &b)
              { return a.tick < b.tick; });

    // Stop all notes on all tracks
    for (auto *track : activeSequence->getTracks()) {
        if (track) track->stopAllNotes();
    }
    
    int last_tick = 0;
    int totalSamplesRendered = 0;

    for (const auto &event : allEvents)
    {
        int ticksToProcess = event.tick - last_tick;
        if (ticksToProcess > 0)
        {
            double durationToRender = nn_ticks_to_seconds(ticksToProcess, project->getPPQ(), project->getTempo());
            int samplesToRender = static_cast<int>(durationToRender * sampleRate);
            if (totalSamplesRendered + samplesToRender > totalSamples)
            {
                samplesToRender = totalSamples - totalSamplesRendered;
            }
            if (samplesToRender > 0)
            {
                dspEngine->render(audioBuffer.data() + totalSamplesRendered * numChannels, samplesToRender, false);
                totalSamplesRendered += samplesToRender;
            }
        }
        
        // Play/stop note through track's synth
        if (event.note.parent) {
            if (event.isNoteOn) {
                event.note.parent->playNote(event.note);
            } else {
                event.note.parent->stopNote(event.note);
            }
            // Process the track's synth queue
            NoteNagaSynthesizer *synth = event.note.parent->getSynth();
            if (synth) {
                synth->processQueue();
            }
        }
        
        last_tick = event.tick;

        // In audio-only mode, we want this signal to drive the main progress bar
        emit audioProgressUpdated((int)((double)totalSamplesRendered * 100 / totalSamples));
    }
    emit audioProgressUpdated(100);

    int remainingSamples = totalSamples - totalSamplesRendered;
    if (remainingSamples > 0)
    {
        dspEngine->render(audioBuffer.data() + totalSamplesRendered * numChannels, remainingSamples, false);
    }
    std::ofstream file(outputPath.toStdString(), std::ios::binary);
    if (!file.is_open())
        return false;
    writeWavHeader(file, sampleRate, totalSamples);
    std::vector<int16_t> intBuffer(audioBuffer.size());
    for (size_t i = 0; i < audioBuffer.size(); ++i)
    {
        intBuffer[i] = static_cast<int16_t>(std::clamp(audioBuffer[i], -1.0f, 1.0f) * 32767.0f);
    }
    file.write(reinterpret_cast<const char *>(intBuffer.data()), intBuffer.size() * sizeof(int16_t));

    return true;
}

bool MediaExporter::exportAudioArrangement(const QString &outputPath)
{
    NoteNagaRuntimeData *project = m_engine->getRuntimeData();
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    NoteNagaArrangement *arrangement = m_arrangement ? m_arrangement : project->getArrangement();
    
    if (!arrangement) return false;
    
    ManualModeGuard manualMode(m_engine, arrangement, project);
    
    const int sampleRate = 44100;
    const int numChannels = 2;
    const int ppq = project->getPPQ();
    const int tempo = project->getTempo();
    
    // Calculate total duration from arrangement
    arrangement->updateMaxTick();
    const double totalDuration = nn_ticks_to_seconds(arrangement->getMaxTick(), ppq, tempo) + 2.0;
    const int totalSamples = static_cast<int>(totalDuration * sampleRate);
    
    std::vector<float> audioBuffer(totalSamples * numChannels, 0.0f);
    
    // Collect all MIDI events from all clips in arrangement
    struct ArrangementMidiEvent
    {
        int tick;           // Absolute tick in arrangement
        NN_Note_t note;
        bool isNoteOn;
        NoteNagaTrack* track;
    };
    std::vector<ArrangementMidiEvent> allEvents;
    
    // Process all arrangement tracks
    for (NoteNagaArrangementTrack* arrTrack : arrangement->getTracks()) {
        if (!arrTrack || arrTrack->isMuted()) continue;
        
        // Process all MIDI clips on this track
        for (const NN_MidiClip_t& clip : arrTrack->getClips()) {
            if (clip.muted) continue;
            
            NoteNagaMidiSeq* seq = project->getSequenceById(clip.sequenceId);
            if (!seq) continue;
            
            int seqLength = seq->getMaxTick();
            if (seqLength <= 0) continue;
            
            // Process all tracks in the sequence
            for (NoteNagaTrack* midiTrack : seq->getTracks()) {
                if (!midiTrack || midiTrack->isMuted() || midiTrack->isTempoTrack()) continue;
                
                // Add notes with looping support
                for (const NN_Note_t& note : midiTrack->getNotes()) {
                    if (!note.start.has_value() || !note.length.has_value()) continue;
                    
                    int noteStart = note.start.value();
                    int noteEnd = noteStart + note.length.value();
                    
                    // Handle looping
                    int loopCount = (clip.durationTicks + seqLength - 1) / seqLength;
                    for (int loop = 0; loop < loopCount; ++loop) {
                        int loopOffset = loop * seqLength;
                        int absNoteStart = clip.startTick + loopOffset + noteStart;
                        int absNoteEnd = clip.startTick + loopOffset + noteEnd;
                        
                        // Clip to clip boundaries
                        int clipEndTick = clip.startTick + clip.durationTicks;
                        if (absNoteEnd <= clip.startTick) continue;
                        if (absNoteStart >= clipEndTick) continue;
                        
                        absNoteStart = std::max(absNoteStart, clip.startTick);
                        absNoteEnd = std::min(absNoteEnd, clipEndTick);
                        
                        if (absNoteEnd > absNoteStart) {
                            // Create a copy of the note with adjusted timing
                            NN_Note_t adjustedNote = note;
                            adjustedNote.start = absNoteStart;
                            adjustedNote.length = absNoteEnd - absNoteStart;
                            
                            allEvents.push_back({absNoteStart, adjustedNote, true, midiTrack});
                            allEvents.push_back({absNoteEnd, adjustedNote, false, midiTrack});
                        }
                    }
                }
            }
        }
    }
    
    // Sort events by tick
    std::sort(allEvents.begin(), allEvents.end(), [](const ArrangementMidiEvent &a, const ArrangementMidiEvent &b) {
        return a.tick < b.tick;
    });
    
    // Stop all notes on all sequences
    for (NoteNagaMidiSeq* seq : project->getSequences()) {
        if (!seq) continue;
        for (auto* track : seq->getTracks()) {
            if (track) track->stopAllNotes();
        }
    }
    
    int last_tick = 0;
    int totalSamplesRendered = 0;
    
    for (const auto &event : allEvents)
    {
        int ticksToProcess = event.tick - last_tick;
        if (ticksToProcess > 0)
        {
            double durationToRender = nn_ticks_to_seconds(ticksToProcess, ppq, tempo);
            int samplesToRender = static_cast<int>(durationToRender * sampleRate);
            if (totalSamplesRendered + samplesToRender > totalSamples)
            {
                samplesToRender = totalSamples - totalSamplesRendered;
            }
            if (samplesToRender > 0)
            {
                dspEngine->render(audioBuffer.data() + totalSamplesRendered * numChannels, samplesToRender, false);
                totalSamplesRendered += samplesToRender;
            }
        }
        
        // Play/stop note through track's synth
        if (event.track) {
            if (event.isNoteOn) {
                event.track->playNote(event.note);
            } else {
                event.track->stopNote(event.note);
            }
            // Process the track's synth queue
            NoteNagaSynthesizer *synth = event.track->getSynth();
            if (synth) {
                synth->processQueue();
            }
        }
        
        last_tick = event.tick;
        emit audioProgressUpdated((int)((double)totalSamplesRendered * 100 / totalSamples));
    }
    emit audioProgressUpdated(100);
    
    int remainingSamples = totalSamples - totalSamplesRendered;
    if (remainingSamples > 0)
    {
        dspEngine->render(audioBuffer.data() + totalSamplesRendered * numChannels, remainingSamples, false);
    }
    
    std::ofstream file(outputPath.toStdString(), std::ios::binary);
    if (!file.is_open())
        return false;
    writeWavHeader(file, sampleRate, totalSamples);
    std::vector<int16_t> intBuffer(audioBuffer.size());
    for (size_t i = 0; i < audioBuffer.size(); ++i)
    {
        intBuffer[i] = static_cast<int16_t>(std::clamp(audioBuffer[i], -1.0f, 1.0f) * 32767.0f);
    }
    file.write(reinterpret_cast<const char *>(intBuffer.data()), intBuffer.size() * sizeof(int16_t));
    
    return true;
}

bool MediaExporter::exportVideoBatched(const QString &outputPath)
{
    // === 1. PHASE: Simulation (Single-thread) ===
    emit statusTextChanged(tr("Simulating effects..."));
    
    NoteNagaRuntimeData* runtimeData = m_engine->getRuntimeData();
    
    // Create one renderer ONLY for simulation - based on source mode
    std::unique_ptr<MediaRenderer> simRendererPtr;
    double totalDuration;
    
    if (m_sourceMode == Arrangement && m_arrangement) {
        simRendererPtr = std::make_unique<MediaRenderer>(m_arrangement, runtimeData);
        m_arrangement->updateMaxTick();
        totalDuration = nn_ticks_to_seconds(m_arrangement->getMaxTick(), runtimeData->getPPQ(), runtimeData->getTempo()) + 1.0;
    } else {
        simRendererPtr = std::make_unique<MediaRenderer>(m_sequence);
        totalDuration = nn_ticks_to_seconds(m_sequence->getMaxTick(), runtimeData->getPPQ(), runtimeData->getTempo()) + 1.0;
    }
    
    MediaRenderer& simRenderer = *simRendererPtr;
    simRenderer.setSecondsVisible(m_secondsVisible);
    simRenderer.setRenderSettings(m_settings);
    simRenderer.prepareKeyboardLayout(m_resolution); // Important for positions!

    m_totalFrames = static_cast<int>(totalDuration * m_fps);
    m_framesRendered = 0;

    std::vector<MediaRenderer::FrameState> allFrameStates;
    allFrameStates.reserve(m_totalFrames);

    MediaRenderer::FrameState lastState; // Start with an empty state

    for (int i = 0; i < m_totalFrames; ++i) {
        double currentTime = static_cast<double>(i) / m_fps;
        double deltaTime = 1.0 / m_fps;
        
        // Calculate the *next* state based on the *last* state
        MediaRenderer::FrameState nextState = simRenderer.calculateNextState(lastState, currentTime, deltaTime);
        
        allFrameStates.push_back(nextState);
        lastState = nextState; // Update lastState for the next iteration
        
        // Progress for simulation (0-10%)
        emit videoProgressUpdated((i + 1) * 10 / m_totalFrames);
    }
    
    // === 2. PHASE: Parallel Rendering ===
    emit statusTextChanged(tr("Rendering video frames..."));
    
    int numThreads = QThread::idealThreadCount();
    // We want at least 10 batches, or one per thread
    int numBatches = std::max(numThreads, 10); 
    int framesPerBatch = (m_totalFrames + numBatches - 1) / numBatches; // Ceiling division

    QFutureSynchronizer<QString> synchronizer;
    QStringList tempBatchFiles;
    
    for (int i = 0; i < numBatches; ++i) {
        int startFrame = i * framesPerBatch;
        int endFrame = std::min(startFrame + framesPerBatch, m_totalFrames);
        if (startFrame >= endFrame) continue;

        // Run the batch render in QtConcurrent
        synchronizer.addFuture(QtConcurrent::run([=]() {
            // Pass 'allFrameStates' by value (copy) for thread safety
            return this->renderVideoBatch(startFrame, endFrame, allFrameStates); 
        }));
    }

    // Wait for all batches to finish rendering
    synchronizer.waitForFinished();
    
    for(const auto& future : synchronizer.futures()) {
        tempBatchFiles.append(future.result());
    }
    
    emit videoProgressUpdated(100);

    // === 3. PHASE: Concatenate ===
    emit statusTextChanged(tr("Joining video files..."));
    if (!concatenateVideos(tempBatchFiles, outputPath)) {
        emit error(tr("Failed to join video batches. FFmpeg error."));
        // Delete temporary batch files
        for(const QString& f : tempBatchFiles) QFile::remove(f);
        return false;
    }
    
    // Delete temporary batch files
    for(const QString& f : tempBatchFiles) QFile::remove(f);

    return true;
}

QString MediaExporter::renderVideoBatch(int startFrame, int endFrame, 
                                        const std::vector<MediaRenderer::FrameState>& allFrameStates)
{
    // Each thread creates its own MediaRenderer - based on source mode
    std::unique_ptr<MediaRenderer> rendererPtr;
    if (m_sourceMode == Arrangement && m_arrangement) {
        rendererPtr = std::make_unique<MediaRenderer>(m_arrangement, m_engine->getRuntimeData());
    } else {
        rendererPtr = std::make_unique<MediaRenderer>(m_sequence);
    }
    
    MediaRenderer& renderer = *rendererPtr;
    renderer.setSecondsVisible(m_secondsVisible);
    renderer.setRenderSettings(m_settings);
    
    // Unique file name for this batch
    QString tempFile = m_outputPath + QString(".tmp.batch.%1.mp4").arg(startFrame);
    
    cv::VideoWriter videoWriter(tempFile.toStdString(), cv::VideoWriter::fourcc('m', 'p', '4', 'v'), m_fps, cv::Size(m_resolution.width(), m_resolution.height()));
    if (!videoWriter.isOpened()) {
        // Can't return an error directly, so return an empty string
        return QString(); 
    }

    for (int i = startFrame; i < endFrame; ++i) {
        double currentTime = static_cast<double>(i) / m_fps;
        
        // Call the stateless renderFrame version
        QImage frame = renderer.renderFrame(currentTime, m_resolution, allFrameStates[i]);

        cv::Mat cvFrame(frame.height(), frame.width(), CV_8UC4, (void*)frame.constBits(), frame.bytesPerLine());
        cv::Mat cvFrameBGR;
        cv::cvtColor(cvFrame, cvFrameBGR, cv::COLOR_BGRA2BGR); // Color correction

        videoWriter.write(cvFrameBGR);
        
        // Update progress (atomically)
        int rendered = m_framesRendered.fetchAndAddOrdered(1);
        // Progress for rendering (10-95%)
        // Emit only occasionally to avoid flooding the GUI
        if (rendered % 20 == 0) { 
             emit videoProgressUpdated(10 + (rendered * 85 / m_totalFrames));
        }
    }

    videoWriter.release();
    return tempFile;
}

bool MediaExporter::concatenateVideos(const QStringList& videoFiles, const QString& finalPath)
{
    // 1. Create 'filelist.txt'
    QString fileListPath = QFileInfo(m_outputPath).dir().filePath("filelist.txt");
    QFile fileList(fileListPath);
    if (!fileList.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&fileList);
    for (const QString& f : videoFiles) {
        // FFmpeg needs ' and escaped backslashes
        QString escapedPath = f;
        escapedPath.replace("\\", "/");
        out << "file '" << escapedPath << "'\n";
    }
    fileList.close();

    // 2. Run FFmpeg
    QProcess ffmpeg;
    QStringList arguments;
    arguments << "-y" << "-f" << "concat" << "-safe" << "0" << "-i" << fileListPath << "-c" << "copy" << finalPath;

    ffmpeg.start("ffmpeg", arguments);
    if (!ffmpeg.waitForStarted()) {
        QFile::remove(fileListPath);
        return false;
    }
    ffmpeg.waitForFinished(-1);
    
    QFile::remove(fileListPath);
    return ffmpeg.exitCode() == 0;
}


bool MediaExporter::combineAudioVideo(const QString &videoPath, const QString &audioPath, const QString &finalPath)
{
    QProcess ffmpeg;
    QStringList arguments;
    arguments << "-y" << "-i" << videoPath << "-i" << audioPath << "-c:v" << "copy" << "-c:a" << "aac" << "-b:a" << "192k" << "-shortest" << finalPath;

    ffmpeg.start("ffmpeg", arguments);
    if (!ffmpeg.waitForStarted())
    {
        return false;
    }
    ffmpeg.waitForFinished(-1);
    return ffmpeg.exitCode() == 0;
}

void MediaExporter::writeWavHeader(std::ofstream &file, int sampleRate, int numSamples)
{
    int numChannels = 2;
    int bitsPerSample = 16;
    int byteRate = sampleRate * numChannels * bitsPerSample / 8;
    int blockAlign = numChannels * bitsPerSample / 8;
    int subchunk2Size = numSamples * numChannels * bitsPerSample / 8;
    int chunkSize = 36 + subchunk2Size;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char *>(&chunkSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    int subchunk1Size = 16;
    file.write(reinterpret_cast<const char *>(&subchunk1Size), 4);
    short audioFormat = 1;
    file.write(reinterpret_cast<const char *>(&audioFormat), 2);
    file.write(reinterpret_cast<const char *>(&numChannels), 2);
    file.write(reinterpret_cast<const char *>(&sampleRate), 4);
    file.write(reinterpret_cast<const char *>(&byteRate), 4);
    file.write(reinterpret_cast<const char *>(&blockAlign), 2);
    file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char *>(&subchunk2Size), 4);
}

bool MediaExporter::transcodeAudio(const QString &inputWavPath, const QString &finalPath, const QString &format, int bitrate)
{
    // When the target format is WAV, simply rename the file
    if (format.toLower() == "wav") {
        QFile::remove(finalPath); 
        return QFile::rename(inputWavPath, finalPath);
    }
    
    QProcess ffmpeg;
    QStringList arguments;
    arguments << "-y" << "-i" << inputWavPath;
    
    if (format.toLower() == "mp3") {
        arguments << "-c:a" << "libmp3lame";
        arguments << "-b:a" << QString::number(bitrate) + "k";
    } else if (format.toLower() == "ogg") {
        arguments << "-c:a" << "libvorbis";
        arguments << "-q:a" << QString::number(bitrate); // For Vorbis, quality is 0-10
        // Recalculation from bitrate (kbps) to quality (q) - rough estimate
        // 64k -> 0, 80k -> 1, 96k -> 2, 112k -> 3, 128k -> 4, 160k -> 5, 192k -> 6, 224k -> 7, 256k -> 8, 320k -> 9
        int q_val = (bitrate - 64) / 32 + 1;
        if (bitrate < 64) q_val = 0;
        if (bitrate > 320) q_val = 10;
        arguments << "-q:a" << QString::number(q_val);
    } else {
        // Fallback for unknown formats (although this shouldn't happen)
        emit error(tr("Unknown audio format: %1").arg(format));
        return false;
    }
    
    arguments << finalPath;

    ffmpeg.start("ffmpeg", arguments);
    if (!ffmpeg.waitForStarted()) {
        return false;
    }
    ffmpeg.waitForFinished(-1);
    return ffmpeg.exitCode() == 0;
}