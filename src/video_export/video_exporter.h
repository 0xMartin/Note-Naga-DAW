#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QFuture>
#include <QFutureWatcher>
#include <QAtomicInt>
#include <note_naga_engine/core/types.h>
#include <note_naga_engine/note_naga_engine.h>

class VideoExporter : public QObject
{
    Q_OBJECT

public:
    explicit VideoExporter(NoteNagaMidiSeq *sequence, QString outputPath,
                           QSize resolution, int fps, NoteNagaEngine *m_engine,
                           double secondsVisible,
                           QObject *parent = nullptr);
    ~VideoExporter();

public slots:
    void doExport();

signals:
    // ZMĚNA: Nové, specifické signály
    void audioProgressUpdated(int percentage);
    void videoProgressUpdated(int percentage);
    void statusTextChanged(const QString &status);
    void finished();
    void error(const QString &errorMessage);

private slots:
    void onTaskFinished();

private:
    NoteNagaEngine *m_engine;
    NoteNagaMidiSeq *m_sequence;
    QString m_outputPath;
    QSize m_resolution;
    int m_fps;
    double m_secondsVisible;

    QFutureWatcher<bool> m_audioWatcher;
    QFutureWatcher<bool> m_videoWatcher;
    QAtomicInt m_finishedTaskCount;

    QString m_tempAudioPath;
    QString m_tempVideoPath;

    bool exportAudio(const QString &outputPath);
    bool exportVideo(const QString &outputPath);
    bool combineAudioVideo(const QString &videoPath, const QString &audioPath, const QString &finalPath);

    void writeWavHeader(std::ofstream &file, int sampleRate, int numSamples);
    void cleanup();
};