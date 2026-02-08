#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QFuture>
#include <QFutureWatcher>
#include <QFutureSynchronizer> 
#include <QAtomicInt>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_engine.h>
#include "media_renderer.h"

class MediaExporter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Export mode enumeration.
     */
    enum ExportMode { Video, AudioOnly };
    
    /**
     * @brief Source mode enumeration.
     */
    enum SourceMode { SingleSequence, Arrangement };

    /**
     * @brief Constructor for single MIDI sequence export.
     */
    explicit MediaExporter(NoteNagaMidiSeq *sequence, 
                           QString outputPath,
                           QSize resolution, 
                           int fps, 
                           NoteNagaEngine *m_engine,
                           double secondsVisible,
                           const MediaRenderer::RenderSettings& settings,
                           ExportMode exportMode, 
                           const QString& audioFormat, 
                           int audioBitrate, 
                           QObject *parent = nullptr);

    /**
     * @brief Constructor for arrangement export.
     */
    explicit MediaExporter(NoteNagaArrangement *arrangement,
                           QString outputPath,
                           QSize resolution, 
                           int fps, 
                           NoteNagaEngine *m_engine,
                           double secondsVisible,
                           const MediaRenderer::RenderSettings& settings,
                           ExportMode exportMode, 
                           const QString& audioFormat, 
                           int audioBitrate, 
                           QObject *parent = nullptr);
                           
    ~MediaExporter();

public slots:
    void doExport();

signals:
    void audioProgressUpdated(int percentage);
    void videoProgressUpdated(int percentage);
    void statusTextChanged(const QString &status);
    void finished();
    void error(const QString &errorMessage);

private slots:
    void onTaskFinished();

private:
    // --- Core components ---
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    NoteNagaArrangement *m_arrangement;
    SourceMode m_sourceMode;

    // --- Export settings ---
    QString m_outputPath;
    QSize m_resolution;
    int m_fps;
    ExportMode m_exportMode;
    QString m_audioFormat;
    int m_audioBitrate;
    
    // --- Stored settings ---
    double m_secondsVisible;
    MediaRenderer::RenderSettings m_settings;

    // --- Export progress ---
    QFutureWatcher<bool> m_audioWatcher;
    QFutureWatcher<bool> m_videoWatcher;
    QAtomicInt m_finishedTaskCount;

    // --- Temporary file paths ---
    QString m_tempAudioPath;
    QString m_tempVideoPath;
    
    // --- Counter for parallel rendering ---
    QAtomicInt m_framesRendered;
    int m_totalFrames;
    
    /**
     * @brief Phase 1: Runs simulation and Phase 2: Parallel rendering.
     */
    bool exportVideoBatched(const QString &outputPath);
    
    /**
     * @brief Renders one batch of frames (runs in its own thread).
     * @return Path to the temporary .mp4 file for this batch.
     */
    QString renderVideoBatch(int startFrame, int endFrame,
                             const std::vector<MediaRenderer::FrameState>& allFrameStates);

    /**
     * @brief Joins multiple video files into one using FFmpeg concat.
     */
    bool concatenateVideos(const QStringList& videoFiles, const QString& outputPath);
    
    /**
     * @brief Transcodes WAV audio to the desired format using FFmpeg.
     * @param inputWavPath Path to the input WAV file.
     * @param finalPath Path to the output audio file.
     * @param format Desired audio format (e.g., "mp3", "aac").
     * @param bitrate Desired audio bitrate (e.g., 192).
     * @return True if transcoding is successful, false otherwise.
     */
    bool transcodeAudio(const QString &inputWavPath, const QString &finalPath, const QString &format, int bitrate);
    
    /**
     * @brief Exports audio to a WAV file.
     * @param outputPath Path to the output WAV file.
     * @return True if export is successful, false otherwise.
     */
    bool exportAudio(const QString &outputPath);
    
    /**
     * @brief Exports audio from arrangement to a WAV file.
     * @param outputPath Path to the output WAV file.
     * @return True if export is successful, false otherwise.
     */
    bool exportAudioArrangement(const QString &outputPath);

    /**
     * @brief Combines audio and video into a final output file.
     * @param videoPath Path to the video file.
     * @param audioPath Path to the audio file.
     * @param finalPath Path to the final output file.
     * @return True if combining is successful, false otherwise.
     */
    bool combineAudioVideo(const QString &videoPath, const QString &audioPath, const QString &finalPath);

    /**
     * @brief Writes a WAV file header.
     * @param file Output file stream.
     * @param sampleRate Sample rate of the audio.
     * @param numSamples Number of samples in the audio data.
     */
    void writeWavHeader(std::ofstream &file, int sampleRate, int numSamples);

    /**
     * @brief Cleans up temporary files.
     */
    void cleanup();

};