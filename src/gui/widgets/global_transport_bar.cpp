#include "global_transport_bar.h"

#include <QIcon>
#include <QInputDialog>
#include <QMouseEvent>
#include <cmath>

#include "../components/midi_seq_progress_bar.h"
#include "../components/track_stereo_meter.h"

GlobalTransportBar::GlobalTransportBar(NoteNagaEngine* engine, QWidget* parent)
    : QFrame(parent)
    , m_engine(engine)
    , m_ppq(0)
    , m_tempo(0)
    , m_maxTick(0)
    , m_wasPlaying(false)
    , m_currentDisplayBPM(120.0)
    , m_isPlaying(false)
    , m_playbackMode(PlaybackMode::Sequence)
{
    setObjectName("GlobalTransportBar");
    initUI();
    setupConnections();
}

void GlobalTransportBar::initUI()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(12);

    // === LEFT: Stereo Meter ===
    m_stereoMeter = new TrackStereoMeter(this);
    m_stereoMeter->setMinimumWidth(90);
    m_stereoMeter->setMaximumWidth(110);
    m_stereoMeter->setFixedHeight(46);
    mainLayout->addWidget(m_stereoMeter);

    // === Transport Controls Group ===
    QStringList btnNames = {"toStartBtn", "playToggleBtn", "toEndBtn"};
    QList<QIcon> btnIcons = {
        QIcon(":/icons/media-backward-end.svg"),
        QIcon(":/icons/play.svg"),
        QIcon(":/icons/media-forward-end.svg")
    };
    QStringList btnTips = {"Go to start", "Play/Stop", "Go to end"};

    m_transportBtnGroup = new ButtonGroupWidget(btnNames, btnIcons, btnTips, QSize(24, 24), false, this);
    connect(m_transportBtnGroup, &ButtonGroupWidget::buttonClicked, this, [this](const QString& btn) {
        if (btn == "toStartBtn") emit goToStart();
        else if (btn == "playToggleBtn") emit playToggled();
        else if (btn == "toEndBtn") emit goToEnd();
    });
    m_transportBtnGroup->button("playToggleBtn")->setCheckable(true);
    mainLayout->addWidget(m_transportBtnGroup);

    // === Playback Mode Toggle (Sequence = unchecked/blue, Arrangement = checked/green) ===
    m_playbackModeBtn = new QPushButton(this);
    m_playbackModeBtn->setObjectName("playbackModeBtn");
    m_playbackModeBtn->setCheckable(true);
    m_playbackModeBtn->setChecked(m_playbackMode == PlaybackMode::Arrangement);
    m_playbackModeBtn->setIcon(QIcon(":/icons/playback-sequence.svg"));
    m_playbackModeBtn->setIconSize(QSize(20, 20));
    m_playbackModeBtn->setToolTip(tr("Sequence Mode - plays selected MIDI sequence"));
    m_playbackModeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_playbackModeBtn, &QPushButton::clicked, this, &GlobalTransportBar::onPlaybackModeToggled);
    mainLayout->addWidget(m_playbackModeBtn);

    // === Progress Bar ===
    m_progressBar = new MidiSequenceProgressBar(this);
    m_progressBar->setObjectName("globalProgressBar");
    m_progressBar->setMinimumWidth(150);
    m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_progressBar->setFixedHeight(32);
    connect(m_progressBar, &MidiSequenceProgressBar::positionPressed, this,
            &GlobalTransportBar::onProgressBarPositionPressed);
    connect(m_progressBar, &MidiSequenceProgressBar::positionDragged, this,
            &GlobalTransportBar::onProgressBarPositionDragged);
    connect(m_progressBar, &MidiSequenceProgressBar::positionReleased, this,
            &GlobalTransportBar::onProgressBarPositionReleased);
    mainLayout->addWidget(m_progressBar, 1);

    // === BPM Display with Metronome ===
    QWidget* bpmWidget = new QWidget(this);
    bpmWidget->setObjectName("bpmWidget");
    QHBoxLayout* bpmLayout = new QHBoxLayout(bpmWidget);
    bpmLayout->setContentsMargins(0, 0, 0, 0);
    bpmLayout->setSpacing(4);

    m_metronomeBtnLbl = new QPushButton(this);
    m_metronomeBtnLbl->setObjectName("metronomeBtn");
    m_metronomeBtnLbl->setCheckable(true);
    m_metronomeBtnLbl->setChecked(m_engine->isMetronomeEnabled());
    m_metronomeBtnLbl->setIcon(QIcon(":/icons/tempo.svg"));
    m_metronomeBtnLbl->setIconSize(QSize(20, 20));
    m_metronomeBtnLbl->setCursor(Qt::PointingHandCursor);
    connect(m_metronomeBtnLbl, &QPushButton::clicked, this, &GlobalTransportBar::metronomeBtnClicked);
    bpmLayout->addWidget(m_metronomeBtnLbl);

    m_tempoLabel = new QLabel("120.0 BPM", this);
    m_tempoLabel->setObjectName("tempoLabel");
    m_tempoLabel->setCursor(Qt::PointingHandCursor);
    m_tempoLabel->setMinimumWidth(75);
    m_tempoLabel->installEventFilter(this);
    bpmLayout->addWidget(m_tempoLabel);

    mainLayout->addWidget(bpmWidget);

    // === Styling ===
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(52);

    setStyleSheet(R"(
        QFrame#GlobalTransportBar {
            background-color: #2a2a30;
            border-top: 1px solid #3a3a42;
        }
        QWidget#bpmWidget {
            background: transparent;
        }
        QLabel#tempoLabel {
            color: #eeeeee;
            border: 1px solid #3a3d45;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
            background-color: #35383f;
            min-height: 24px;
            max-height: 24px;
        }
        QLabel#tempoLabel:hover {
            background-color: #404550;
            border-color: #5a5d65;
        }
        QPushButton#metronomeBtn {
            padding: 0px;
            border-radius: 6px;
            background: #253a4c;
            border: 1.5px solid #4866a0;
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
        }
        QPushButton#metronomeBtn:checked {
            background: #3477c0;
            border: 1.5px solid #79b8ff;
        }
        QPushButton#metronomeBtn:hover {
            background: #29528c;
            border: 1.5px solid #79b8ff;
        }
        QPushButton#playbackModeBtn {
            padding: 0px;
            border-radius: 6px;
            background: #253a4c;
            border: 1.5px solid #4866a0;
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
        }
        QPushButton#playbackModeBtn:checked {
            background: #3c5a3c;
            border: 1.5px solid #5a9a5a;
        }
        QPushButton#playbackModeBtn:hover:!checked {
            background: #29528c;
            border: 1.5px solid #79b8ff;
        }
        QPushButton#playbackModeBtn:hover:checked {
            background: #4a6d4a;
            border: 1.5px solid #6a6d75;
        }
    )");
}

void GlobalTransportBar::setupConnections()
{
    // Connect to runtime data signals
    NoteNagaRuntimeData* runtimeData = m_engine->getRuntimeData();

    connect(runtimeData, &NoteNagaRuntimeData::activeSequenceChanged, this,
            [this](NoteNagaMidiSeq* seq) {
                if (seq) {
                    m_ppq = seq->getPPQ();
                    m_tempo = seq->getTempo();
                    m_maxTick = seq->getMaxTick();
                    m_progressBar->setMidiSequence(seq);
                }
                updateProgressBar();
                updateBPM();
            });

    connect(runtimeData, &NoteNagaRuntimeData::sequenceMetadataChanged, this,
            [this](NoteNagaMidiSeq* seq, const std::string& param) {
                if (seq) {
                    m_ppq = seq->getPPQ();
                    m_tempo = seq->getTempo();
                    m_maxTick = seq->getMaxTick();
                    
                    // Check if this is the sequence we're displaying
                    NoteNagaMidiSeq* currentSeq = m_engine->getRuntimeData()->getActiveSequence();
                    if (seq == currentSeq) {
                        // Always refresh waveform when notes change
                        if (param == "notes" || param == "note_added" || param == "note_removed" || param == "note_modified") {
                            // Update max time first, then schedule async waveform refresh
                            m_progressBar->updateMaxTime();
                            m_progressBar->scheduleWaveformRefresh();
                        } else {
                            m_progressBar->setMidiSequence(seq);
                        }
                    }
                }
                updateProgressBar();
                updateBPM();
            });
    
    // CRITICAL: Track-level note changes - this is what fires when notes are added/removed
    connect(runtimeData, &NoteNagaRuntimeData::trackMetaChanged, this,
            [this](NoteNagaTrack* track, const std::string& param) {
                if (!track) return;
                
                // Only care about note changes
                if (param == "notes" || param == "note_added" || param == "note_removed" || param == "note_modified") {
                    NoteNagaMidiSeq* currentSeq = m_engine->getRuntimeData()->getActiveSequence();
                    if (currentSeq) {
                        // Check if this track belongs to the current sequence
                        const auto& tracks = currentSeq->getTracks();
                        for (const auto* t : tracks) {
                            if (t == track) {
                                // Recalculate max tick and refresh waveform
                                currentSeq->computeMaxTick();
                                m_maxTick = currentSeq->getMaxTick();
                                m_progressBar->updateMaxTime();
                                m_progressBar->scheduleWaveformRefresh();
                                updateProgressBar();
                                break;
                            }
                        }
                    }
                }
            });

    connect(runtimeData, &NoteNagaRuntimeData::currentTickChanged, this,
            [this]() {
                if (m_playbackMode == PlaybackMode::Sequence) {
                    updateProgressBar();
                }
            });
    
    // Also listen to arrangement tick changes for arrangement mode
    connect(runtimeData, &NoteNagaRuntimeData::currentArrangementTickChanged, this,
            [this]() {
                if (m_playbackMode == PlaybackMode::Arrangement) {
                    updateProgressBar();
                }
            });
    
    // Listen to arrangement tempo track changes
    NoteNagaArrangement* arrangement = runtimeData->getArrangement();
    if (arrangement) {
        connect(arrangement, &NoteNagaArrangement::tempoTrackChanged, this,
                [this]() { updateBPM(); });
    }

    connect(runtimeData, &NoteNagaRuntimeData::currentTempoChanged, this,
            [this](double bpm) { updateCurrentTempo(bpm); });

    // Playback state signals
    connect(m_engine, &NoteNagaEngine::playbackStarted, this, [this]() {
        setPlaying(true);
        m_isPlaying = true;
    });

    connect(m_engine, &NoteNagaEngine::playbackStopped, this, [this]() {
        setPlaying(false);
        m_isPlaying = false;
        updateBPM();
        updateProgressBar();  // Update progress bar to show correct position after stop
    });
    
    // Initialize with current active sequence if exists
    NoteNagaMidiSeq* activeSeq = runtimeData->getActiveSequence();
    if (activeSeq) {
        m_ppq = activeSeq->getPPQ();
        m_tempo = activeSeq->getTempo();
        m_maxTick = activeSeq->getMaxTick();
        m_progressBar->setMidiSequence(activeSeq);
        updateProgressBar();
        updateBPM();
    }
}

void GlobalTransportBar::updateCurrentTempo(double bpm)
{
    if (!m_isPlaying) return;

    m_currentDisplayBPM = bpm;
    
    bool hasActiveTempoTrack = false;
    
    if (m_playbackMode == PlaybackMode::Arrangement) {
        NoteNagaArrangement* arr = m_engine->getRuntimeData()->getArrangement();
        hasActiveTempoTrack = arr && arr->hasTempoTrack() && arr->getTempoTrack()->isTempoTrackActive();
    } else {
        NoteNagaMidiSeq* seq = m_engine->getRuntimeData()->getActiveSequence();
        hasActiveTempoTrack = seq && seq->hasTempoTrack() && seq->getTempoTrack()->isTempoTrackActive();
    }

    if (hasActiveTempoTrack) {
        m_tempoLabel->setText(QString("♪ %1 BPM").arg(bpm, 0, 'f', 1));
    } else {
        m_tempoLabel->setText(QString("%1 BPM").arg(bpm, 0, 'f', 1));
    }
}

void GlobalTransportBar::updateBPM()
{
    NoteNagaRuntimeData* project = m_engine->getRuntimeData();
    
    bool hasActiveTempoTrack = false;
    double bpm = 120.0;
    
    if (m_playbackMode == PlaybackMode::Arrangement) {
        // Arrangement mode - check arrangement tempo track
        NoteNagaArrangement* arr = project->getArrangement();
        hasActiveTempoTrack = arr && arr->hasTempoTrack() && arr->getTempoTrack()->isTempoTrackActive();
        
        if (hasActiveTempoTrack && m_isPlaying) {
            bpm = m_currentDisplayBPM;
        } else if (hasActiveTempoTrack) {
            bpm = arr->getEffectiveBPMAtTick(project->getCurrentArrangementTick());
        } else {
            // Use project base tempo (not sequence tempo) for arrangement mode
            int projectTempo = project->getProjectTempo();
            bpm = projectTempo ? (60'000'000.0 / double(projectTempo)) : 120.0;
        }
    } else {
        // Sequence mode - check sequence tempo track
        NoteNagaMidiSeq* seq = project->getActiveSequence();
        hasActiveTempoTrack = seq && seq->hasTempoTrack() && seq->getTempoTrack()->isTempoTrackActive();

        if (hasActiveTempoTrack && m_isPlaying) {
            bpm = m_currentDisplayBPM;
        } else if (hasActiveTempoTrack) {
            bpm = seq->getEffectiveBPMAtTick(project->getCurrentTick());
        } else {
            bpm = project->getTempo() ? (60'000'000.0 / double(project->getTempo())) : 120.0;
        }
    }

    m_currentDisplayBPM = bpm;

    if (hasActiveTempoTrack) {
        m_tempoLabel->setText(QString("♪ %1 BPM").arg(bpm, 0, 'f', 1));
        m_tempoLabel->setToolTip(tr("Tempo Track Active - Dynamic tempo control enabled"));
        m_tempoLabel->setCursor(Qt::ArrowCursor);
    } else {
        m_tempoLabel->setText(QString("%1 BPM").arg(bpm, 0, 'f', 1));
        m_tempoLabel->setToolTip(tr("Click to change tempo"));
        m_tempoLabel->setCursor(Qt::PointingHandCursor);
    }

    m_progressBar->updateMaxTime();
}

void GlobalTransportBar::updateProgressBar()
{
    NoteNagaRuntimeData* project = m_engine->getRuntimeData();
    if (m_ppq == 0 || m_tempo == 0) return;
    
    if (m_playbackMode == PlaybackMode::Arrangement) {
        // In arrangement mode, show arrangement position and total time
        NoteNagaArrangement *arrangement = project->getArrangement();
        if (arrangement) {
            int currentTick = project->getCurrentArrangementTick();
            int maxTick = arrangement->getMaxTick();
            
            // Use project tempo for arrangement
            int projectTempo = project->getTempo();
            int projectPpq = project->getPPQ();
            if (projectTempo == 0 || projectPpq == 0) return;
            
            double usPerTick = double(projectTempo) / double(projectPpq);
            double curSec = double(currentTick) * usPerTick / 1'000'000.0;
            double totalSec = double(maxTick) * usPerTick / 1'000'000.0;
            
            // Update progress bar with arrangement data
            m_progressBar->setTotalTime(std::max(1.0f, static_cast<float>(totalSec)));
            m_progressBar->setCurrentTime(curSec);
            return;
        }
    }
    
    // Default: sequence mode
    double usPerTick = double(m_tempo) / double(m_ppq);
    double curSec = double(project->getCurrentTick()) * usPerTick / 1'000'000.0;
    m_progressBar->setCurrentTime(curSec);
}

void GlobalTransportBar::setPlaying(bool is_playing)
{
    QPushButton* playBtn = m_transportBtnGroup->button("playToggleBtn");
    if (!playBtn) return;

    if (is_playing) {
        playBtn->setIcon(QIcon(":/icons/stop.svg"));
        playBtn->setChecked(true);
    } else {
        playBtn->setIcon(QIcon(":/icons/play.svg"));
        playBtn->setChecked(false);
    }
    playBtn->update();
}

void GlobalTransportBar::setPlaybackMode(PlaybackMode mode)
{
    if (m_playbackMode != mode) {
        m_playbackMode = mode;

        if (mode == PlaybackMode::Sequence) {
            m_playbackModeBtn->setIcon(QIcon(":/icons/playback-sequence.svg"));
            m_playbackModeBtn->setToolTip(tr("Sequence Mode - plays selected MIDI sequence"));
            m_playbackModeBtn->setChecked(false);
            m_progressBar->setArrangementMode(false);
        } else {
            m_playbackModeBtn->setIcon(QIcon(":/icons/playback-compose.svg"));
            m_playbackModeBtn->setToolTip(tr("Arrangement Mode - plays full timeline/composition"));
            m_playbackModeBtn->setChecked(true);
            m_progressBar->setArrangementMode(true);
        }

        emit playbackModeChanged(mode);
    }
}

void GlobalTransportBar::onPlaybackModeToggled()
{
    // Check if toggling is allowed based on current section constraints
    if (m_playbackMode == PlaybackMode::Sequence) {
        // Trying to switch to Arrangement mode
        if (!(m_allowedPlaybackModes & 2)) {
            // Arrangement mode not allowed in this section
            return;
        }
        setPlaybackMode(PlaybackMode::Arrangement);
    } else {
        // Trying to switch to Sequence mode
        if (!(m_allowedPlaybackModes & 1)) {
            // Sequence mode not allowed in this section
            return;
        }
        setPlaybackMode(PlaybackMode::Sequence);
    }
}

void GlobalTransportBar::setAllowedPlaybackModes(int allowedModes)
{
    m_allowedPlaybackModes = allowedModes;
    
    // Update button enabled state based on allowed modes
    if (m_playbackModeBtn) {
        // If only one mode is allowed, disable the toggle button
        bool canToggle = (allowedModes == 3);
        m_playbackModeBtn->setEnabled(canToggle);
        
        if (!canToggle) {
            // Force to the allowed mode
            if ((allowedModes & 1) && m_playbackMode != PlaybackMode::Sequence) {
                setPlaybackMode(PlaybackMode::Sequence);
            } else if ((allowedModes & 2) && m_playbackMode != PlaybackMode::Arrangement) {
                setPlaybackMode(PlaybackMode::Arrangement);
            }
        }
    }
}

void GlobalTransportBar::editTempo(QMouseEvent* event)
{
    NoteNagaRuntimeData* project = m_engine->getRuntimeData();
    if (!project) return;
    
    if (m_playbackMode == PlaybackMode::Arrangement) {
        // Arrangement mode - check arrangement tempo track
        NoteNagaArrangement* arrangement = project->getArrangement();
        if (!arrangement) return;
        
        // Don't allow editing if arrangement tempo track is active
        if (arrangement->hasTempoTrack() && arrangement->getTempoTrack()->isTempoTrackActive()) {
            return;
        }
        
        // Use project base tempo (not sequence tempo) for arrangement mode
        int projectTempo = project->getProjectTempo();
        double curBpm = projectTempo ? (60'000'000.0 / double(projectTempo)) : 120.0;
        
        QString dialogTitle = tr("Change Tempo");
        QString dialogLabel = tr("New Tempo (BPM):");
        if (arrangement->hasTempoTrack()) {
            dialogTitle = tr("Change Fixed Tempo");
            dialogLabel = tr("Fixed Tempo (BPM) - Tempo track is inactive:");
        }
        
        bool ok = false;
        double bpm = QInputDialog::getDouble(this, dialogTitle, dialogLabel, curBpm, 5, 500, 2, &ok);
        if (ok) {
            int newTempo = 60'000'000.0 / bpm;
            project->setTempo(newTempo);
            updateBPM();
            updateProgressBar();
            m_engine->changeTempo(newTempo);
            emit tempoChanged(newTempo);
        }
    } else {
        // Sequence mode - check sequence tempo track
        NoteNagaMidiSeq* seq = project->getActiveSequence();
        if (!seq) return;

        // Don't allow editing if tempo track is active
        if (seq->hasTempoTrack() && seq->getTempoTrack()->isTempoTrackActive()) {
            return;
        }

        double curBpm = seq->getTempo() ? (60'000'000.0 / double(seq->getTempo())) : 120.0;

        QString dialogTitle = tr("Change Tempo");
        QString dialogLabel = tr("New Tempo (BPM):");
        if (seq->hasTempoTrack()) {
            dialogTitle = tr("Change Fixed Tempo");
            dialogLabel = tr("Fixed Tempo (BPM) - Tempo track is inactive:");
        }

        bool ok = false;
        double bpm = QInputDialog::getDouble(this, dialogTitle, dialogLabel, curBpm, 5, 500, 2, &ok);
        if (ok) {
            int newTempo = 60'000'000.0 / bpm;
            seq->setTempo(newTempo);
            updateBPM();
            updateProgressBar();
            m_engine->changeTempo(newTempo);
            emit tempoChanged(newTempo);
        }
    }
}

bool GlobalTransportBar::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_tempoLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        editTempo(mouseEvent);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void GlobalTransportBar::metronomeBtnClicked()
{
    bool metronomeOn = m_metronomeBtnLbl->isChecked();
    m_engine->enableMetronome(metronomeOn);
    emit metronomeToggled(metronomeOn);
}

void GlobalTransportBar::onProgressBarPositionPressed(float seconds)
{
    NoteNagaRuntimeData* project = m_engine->getRuntimeData();
    if (!project) return;

    m_wasPlaying = m_engine->isPlaying();
    if (m_wasPlaying) m_engine->stopPlayback();
    
    int tickPosition = 0;
    if (m_playbackMode == PlaybackMode::Arrangement) {
        // In arrangement mode, use project tempo/ppq for tick calculation
        int projectTempo = project->getTempo();
        int projectPpq = project->getPPQ();
        if (projectTempo == 0 || projectPpq == 0) return;
        
        tickPosition = nn_seconds_to_ticks(seconds, projectPpq, projectTempo);
        project->setCurrentArrangementTick(tickPosition);
    } else {
        // Sequence mode
        if (m_ppq == 0 || m_tempo == 0) return;
        tickPosition = nn_seconds_to_ticks(seconds, m_ppq, m_tempo);
        project->setCurrentTick(tickPosition);
    }

    updateProgressBar();

    emit playPositionChanged(seconds, tickPosition);
}

void GlobalTransportBar::onProgressBarPositionDragged(float seconds)
{
    NoteNagaRuntimeData* project = m_engine->getRuntimeData();
    if (!project) return;

    int tickPosition = 0;
    if (m_playbackMode == PlaybackMode::Arrangement) {
        // In arrangement mode, use project tempo/ppq for tick calculation
        int projectTempo = project->getTempo();
        int projectPpq = project->getPPQ();
        if (projectTempo == 0 || projectPpq == 0) return;
        
        tickPosition = nn_seconds_to_ticks(seconds, projectPpq, projectTempo);
        project->setCurrentArrangementTick(tickPosition);
    } else {
        // Sequence mode
        if (m_ppq == 0 || m_tempo == 0) return;
        tickPosition = nn_seconds_to_ticks(seconds, m_ppq, m_tempo);
        project->setCurrentTick(tickPosition);
    }

    updateProgressBar();

    emit playPositionChanged(seconds, tickPosition);
}

void GlobalTransportBar::onProgressBarPositionReleased(float seconds)
{
    if (m_wasPlaying) m_engine->startPlayback();
}
