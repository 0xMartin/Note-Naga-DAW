#include "video_exporter.h"

#include "video_renderer.h"
#include <note_naga_engine/note_naga_engine.h>
#include <opencv2/opencv.hpp>
#include <QImage>
#include <fstream>
#include <QProcess>
#include <QFileInfo>

/**************************************************************************************/
// Util class to manage manual mode for engine components
/**************************************************************************************/
class ManualModeGuard {
public:
    ManualModeGuard(NoteNagaEngine* engine) : m_engine(engine) {
        if (m_engine) {
            m_engine->getMixer()->enterManualMode();
            for (auto* synth : m_engine->getSynthesizers()) {
                synth->enterManualMode();
            }
        }
    }
    ~ManualModeGuard() {
        if (m_engine) {
            m_engine->getMixer()->exitManualMode();
            for (auto* synth : m_engine->getSynthesizers()) {
                synth->exitManualMode();
            }
        }
    }
private:
    NoteNagaEngine* m_engine;
};

/**************************************************************************************/
// VideoExporter implementation
/**************************************************************************************/

VideoExporter::VideoExporter(NoteNagaMidiSeq *sequence, QString outputPath,
                             QSize resolution, int fps, NoteNagaEngine *m_engine, double secondsVisible, QObject *parent)
    : QObject(parent), m_sequence(sequence), m_outputPath(outputPath), m_resolution(resolution), m_fps(fps), m_engine(m_engine), m_secondsVisible(secondsVisible) {}

void VideoExporter::doExport()
{
    QString tempAudioPath = m_outputPath + ".tmp.wav";
    QString tempVideoPath = m_outputPath + ".tmp.mp4";

    // Fáze 1: Export audia
    emit progressUpdated(0, "Fáze 1/3: Renderování zvuku...");
    if (!exportAudio(tempAudioPath))
    {
        emit error("Nepodařilo se vyrenderovat zvuk.");
        return;
    }

    // Fáze 2: Export videa
    emit progressUpdated(0, "Fáze 2/3: Renderování videa...");
    if (!exportVideo(tempVideoPath))
    {
        emit error("Nepodařilo se vyrenderovat video.");
        QFile::remove(tempAudioPath);
        return;
    }

    // Fáze 3: Spojení audia a videa
    emit progressUpdated(0, "Fáze 3/3: Spojování souborů (muxing)...");
    if (!combineAudioVideo(tempVideoPath, tempAudioPath, m_outputPath))
    {
        emit error("Nepodařilo se spojit video a zvuk. Je FFmpeg nainstalován a v systémové cestě (PATH)?");
    }

    // Úklid dočasných souborů
    QFile::remove(tempAudioPath);
    QFile::remove(tempVideoPath);

    emit finished();
}

bool VideoExporter::exportAudio(const QString &outputPath)
{
    ManualModeGuard manualMode(this->m_engine);

    // --- Inicializace ---
    const int sampleRate = 44100;
    const int numChannels = 2;
    const double totalDuration = nn_ticks_to_seconds(m_engine->getProject()->getActiveSequence()->getMaxTick(), m_engine->getProject()->getPPQ(), m_engine->getProject()->getTempo()) + 2.0; // Přidáme rezervu
    const int totalSamples = static_cast<int>(totalDuration * sampleRate);

    std::vector<float> audioBuffer(totalSamples * numChannels, 0.0f);

    NoteNagaProject *project = m_engine->getProject();
    NoteNagaMidiSeq *activeSequence = project->getActiveSequence();
    NoteNagaMixer *mixer = m_engine->getMixer();
    NoteNagaDSPEngine *dspEngine = m_engine->getDSPEngine();
    auto synthesizers = m_engine->getSynthesizers();

    // --- Krok 1: Vytvoření a seřazení všech MIDI událostí ---
    struct MidiEvent {
        int tick;
        NN_Note_t note;
        bool isNoteOn;
    };

    std::vector<MidiEvent> allEvents;
    for (auto *track : activeSequence->getTracks()) {
        if (track->isMuted() || (activeSequence->getSoloTrack() && activeSequence->getSoloTrack() != track)) continue;
        for (const auto &note : track->getNotes()) {
            if (note.start.has_value() && note.length.has_value()) {
                allEvents.push_back({note.start.value(), note, true}); // Note On
                allEvents.push_back({note.start.value() + note.length.value(), note, false}); // Note Off
            }
        }
    }

    // Seřadíme všechny události chronologicky podle času (ticku)
    std::sort(allEvents.begin(), allEvents.end(), [](const MidiEvent &a, const MidiEvent &b) {
        return a.tick < b.tick;
    });

    // --- Krok 2: Renderování řízené událostmi ---
    mixer->stopAllNotes(); 
    project->setCurrentTick(0);
    int last_tick = 0;
    int totalSamplesRendered = 0;

    for (const auto& event : allEvents) {
        // Vypočítáme, kolik ticků uplynulo od poslední události
        int ticksToProcess = event.tick - last_tick;
        if (ticksToProcess > 0) {
            // Převedeme ticky na samply a vyrenderujeme "ticho" nebo dojezd not
            double durationToRender = nn_ticks_to_seconds(ticksToProcess, project->getPPQ(), project->getTempo());
            int samplesToRender = static_cast<int>(durationToRender * sampleRate);

            // Ujistíme se, že nepřetečeme buffer
            if (totalSamplesRendered + samplesToRender > totalSamples) {
                samplesToRender = totalSamples - totalSamplesRendered;
            }

            if (samplesToRender > 0) {
                dspEngine->render(audioBuffer.data() + totalSamplesRendered * numChannels, samplesToRender);
                totalSamplesRendered += samplesToRender;
            }
        }
        
        // Zpracujeme aktuální MIDI událost (Note On nebo Note Off)
        if (event.isNoteOn) {
            mixer->playNote(event.note);
        } else {
            mixer->stopNote(event.note);
        }
        
        last_tick = event.tick;
        emit progressUpdated((int)((double)totalSamplesRendered / totalSamples * 100), tr("Rendering audio..."));
    }

    // Zpracování front po každé události je klíčové
    mixer->flushNotes();
    mixer->processQueue();
    for (auto* synth : synthesizers) {
        synth->processQueue();
    }

    // Drenderujeme zbytek do konce (dojezd posledních not)
    int remainingSamples = totalSamples - totalSamplesRendered;
    if (remainingSamples > 0) {
        dspEngine->render(audioBuffer.data() + totalSamplesRendered * numChannels, remainingSamples);
    }

    // --- Krok 3: Uložení do .wav souboru ---
    std::ofstream file(outputPath.toStdString(), std::ios::binary);
    if (!file.is_open()) return false;

    writeWavHeader(file, sampleRate, totalSamples);

    std::vector<int16_t> intBuffer(audioBuffer.size());
    for (size_t i = 0; i < audioBuffer.size(); ++i) {
        intBuffer[i] = static_cast<int16_t>(std::clamp(audioBuffer[i], -1.0f, 1.0f) * 32767.0f);
    }
    file.write(reinterpret_cast<const char *>(intBuffer.data()), intBuffer.size() * sizeof(int16_t));

    return true;
}

bool VideoExporter::exportVideo(const QString &outputPath)
{
    VideoRenderer renderer(m_engine->getProject()->getActiveSequence());
    renderer.setSecondsVisible(m_secondsVisible);
    cv::VideoWriter videoWriter(outputPath.toStdString(), cv::VideoWriter::fourcc('m', 'p', '4', 'v'), m_fps, cv::Size(m_resolution.width(), m_resolution.height()));

    if (!videoWriter.isOpened())
        return false;

    double totalDuration = nn_ticks_to_seconds(m_engine->getProject()->getActiveSequence()->getMaxTick(), m_engine->getProject()->getPPQ(), m_engine->getProject()->getTempo()) + 1.0;
    int totalFrames = static_cast<int>(totalDuration * m_fps);

    for (int i = 0; i < totalFrames; ++i)
    {
        double currentTime = static_cast<double>(i) / m_fps;
        QImage frame = renderer.renderFrame(currentTime, m_resolution);
        cv::Mat cvFrame(frame.height(), frame.width(), CV_8UC4, (void *)frame.constBits(), frame.bytesPerLine());
        cv::Mat cvFrameBGR;
        cv::cvtColor(cvFrame, cvFrameBGR, cv::COLOR_RGBA2BGR);
        videoWriter.write(cvFrameBGR);
        emit progressUpdated((int)((double)i / totalFrames * 100), "Renderování videa...");
    }
    videoWriter.release();
    return true;
}

bool VideoExporter::combineAudioVideo(const QString &videoPath, const QString &audioPath, const QString &finalPath)
{
    QProcess ffmpeg;
    QStringList arguments;
    arguments << "-y" << "-i" << videoPath << "-i" << audioPath << "-c:v" << "copy" << "-c:a" << "aac" << "-b:a" << "192k" << "-shortest" << finalPath;

    ffmpeg.start("ffmpeg", arguments);
    if (!ffmpeg.waitForStarted())
    {
        return false;
    }
    ffmpeg.waitForFinished(-1); // Čekej neomezeně
    return ffmpeg.exitCode() == 0;
}

void VideoExporter::writeWavHeader(std::ofstream &file, int sampleRate, int numSamples)
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
    short audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char *>(&audioFormat), 2);
    file.write(reinterpret_cast<const char *>(&numChannels), 2);
    file.write(reinterpret_cast<const char *>(&sampleRate), 4);
    file.write(reinterpret_cast<const char *>(&byteRate), 4);
    file.write(reinterpret_cast<const char *>(&blockAlign), 2);
    file.write(reinterpret_cast<const char *>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char *>(&subchunk2Size), 4);
}