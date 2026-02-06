#include "track_list_widget.h"

#include "../dialogs/instrument_selector_dialog.h"
#include "../nn_gui_utils.h"
#include "note_naga_engine/core/types.h"
#include "note_naga_engine/synth/synth_fluidsynth.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>

TrackListWidget::TrackListWidget(NoteNagaEngine *engine_, QWidget *parent)
    : QWidget(parent), engine(engine_), selected_row(-1) {
  this->title_widget = nullptr;
  this->meter_update_timer_ = nullptr;
  initTitleUI();
  initUI();

  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  reloadTracks(seq);

  // Signals
  connect(engine->getRuntimeData(), &NoteNagaRuntimeData::activeSequenceChanged, this,
          &TrackListWidget::reloadTracks);
  connect(engine->getRuntimeData(),
          &NoteNagaRuntimeData::activeSequenceTrackListChanged, this,
          &TrackListWidget::reloadTracks);

  // Timer to update track stereo meters
  meter_update_timer_ = new QTimer(this);
  connect(meter_update_timer_, &QTimer::timeout, this, &TrackListWidget::updateTrackMeters);
  meter_update_timer_->start(50); // 20 fps
}

void TrackListWidget::initTitleUI() {
  if (this->title_widget)
    return;
  this->title_widget = new QWidget();
  QHBoxLayout *layout = new QHBoxLayout(title_widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  QPushButton *btn_add =
      create_small_button(":/icons/add.svg", "Add new Track", "AddButton");

  QPushButton *btn_add_tempo =
      create_small_button(":/icons/tempo.svg", "Add Tempo Track", "AddTempoButton");

  QPushButton *btn_remove = create_small_button(
      ":/icons/remove.svg", "Remove selected Track", "RemoveButton");

  QPushButton *btn_clear = create_small_button(
      ":/icons/clear.svg", "Clear all Tracks", "ClearButton");

  QPushButton *btn_reload = create_small_button(
      ":/icons/reload.svg", "Reload Tracks from MIDI", "ReloadButton");

  QPushButton *btn_reload_sf = create_small_button(
      ":/icons/audio-signal.svg", "Set SoundFont for all tracks", "SetGlobalSFButton");

  layout->addWidget(btn_add, 0, Qt::AlignRight);
  layout->addWidget(btn_add_tempo, 0, Qt::AlignRight);
  layout->addWidget(btn_remove, 0, Qt::AlignRight);
  layout->addWidget(btn_clear, 0, Qt::AlignRight);
  layout->addWidget(btn_reload, 0, Qt::AlignRight);
  layout->addWidget(btn_reload_sf, 0, Qt::AlignRight);

  connect(btn_add, &QPushButton::clicked, this, &TrackListWidget::onAddTrack);
  connect(btn_add_tempo, &QPushButton::clicked, this, &TrackListWidget::onAddTempoTrack);
  connect(btn_remove, &QPushButton::clicked, this,
          &TrackListWidget::onRemoveTrack);
  connect(btn_clear, &QPushButton::clicked, this,
          &TrackListWidget::onClearTracks);
  connect(btn_reload, &QPushButton::clicked, this,
          &TrackListWidget::onReloadTracks);
  connect(btn_reload_sf, &QPushButton::clicked, this,
          &TrackListWidget::onReloadAllSoundFonts);
}

void TrackListWidget::initUI() {
  scroll_area = new QScrollArea(this);
  scroll_area->setWidgetResizable(true);
  scroll_area->setFrameShape(QFrame::NoFrame);
  scroll_area->setStyleSheet(
      "QScrollArea { background: transparent; padding: 0px; border: none; }");

  container = new QWidget();
  vbox = new QVBoxLayout(container);
  vbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(0);
  vbox->addStretch(1);

  scroll_area->setWidget(container);

  // --- Layout pro celý widget ---
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(5, 5, 5, 5);
  main_layout->addWidget(scroll_area, 1);
  setLayout(main_layout);
}

void TrackListWidget::reloadTracks(NoteNagaMidiSeq *seq) {
  // Remove all widgets and any existing stretch from the layout
  while (vbox->count() > 0) {
    QLayoutItem *item = vbox->takeAt(vbox->count() - 1);
    QWidget *widget = item->widget();
    if (widget != nullptr) {
      widget->deleteLater();
    }
    delete item;
  }
  track_widgets.clear();

  if (!seq) {
    selected_row = -1;
    return;
  }

  for (size_t idx = 0; idx < seq->getTracks().size(); ++idx) {
    NoteNagaTrack *track = seq->getTracks()[idx];
    if (!track)
      continue;
    TrackWidget *widget = new TrackWidget(this->engine, track, container);

    // Selection handling via event filter
    widget->installEventFilter(this);
    widget->setMouseTracking(true);
    widget->refreshStyle(false, idx % 2 == 0);
    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Context menu connection
    connect(widget, &QWidget::customContextMenuRequested, this, 
            [this, widget](const QPoint &pos) {
      showTrackContextMenu(widget, widget->mapToGlobal(pos));
    });

    // Custom mousePressEvent via subclass or signal (see below)
    connect(widget, &TrackWidget::clicked, this, [this, seq, idx]() {
      updateSelection(seq, static_cast<int>(idx));
    });
    
    // Forward solo view signal and track the solo view state
    connect(widget, &TrackWidget::soloViewToggled, this, [this, track](NoteNagaTrack *t, bool enabled) {
      // Track which track has solo view active
      solo_view_track_ = enabled ? t : nullptr;
      emit soloViewToggled(t, enabled);
    });
    
    // Restore solo view button state if this track was in solo view
    if (solo_view_track_ == track) {
      widget->setSoloViewChecked(true);
    }

    track_widgets.push_back(widget);
    vbox->addWidget(widget);
  }
  vbox->addStretch();
  updateSelection(seq, track_widgets.empty() ? -1 : 0);
}

void TrackListWidget::updateSelection(NoteNagaMidiSeq *sequence,
                                      int widget_idx) {
  selected_row = widget_idx;
  for (size_t i = 0; i < track_widgets.size(); ++i) {
    track_widgets[i]->refreshStyle(static_cast<int>(i) == widget_idx,
                                   i % 2 == 0);
    if (static_cast<int>(i) == widget_idx) {
      sequence->setActiveTrack(track_widgets[i]->getTrack());
    }
  }
}

void TrackListWidget::selectAndScrollToTrack(NoteNagaTrack *track) {
  if (!track) return;
  
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) return;
  
  // Find the widget index for this track
  for (int i = 0; i < (int)track_widgets.size(); ++i) {
    if (track_widgets[i]->getTrack() == track) {
      // Update selection
      updateSelection(seq, i);
      
      // Scroll to make the widget visible
      scroll_area->ensureWidgetVisible(track_widgets[i], 0, 50);
      break;
    }
  }
}

void TrackListWidget::onAddTrack() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
    return;
  }
  
  // Prevent adding tracks during playback
  if (engine->isPlaying()) {
    QMessageBox::warning(this, "Playback Active",
                         "Cannot add tracks during playback. Please stop playback first.");
    return;
  }

  InstrumentSelectorDialog dlg(this, GM_INSTRUMENTS, instrument_icon);
  if (dlg.exec() == QDialog::Accepted) {
    int selected_gm_index = dlg.getSelectedGMIndex();
    if (selected_gm_index >= 0) {
      seq->addTrack(selected_gm_index);
    }
  }
}

void TrackListWidget::onAddTempoTrack() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
    return;
  }
  
  // Prevent adding tracks during playback
  if (engine->isPlaying()) {
    QMessageBox::warning(this, "Playback Active",
                         "Cannot add tracks during playback. Please stop playback first.");
    return;
  }
  
  // Check if tempo track already exists
  for (auto *track : seq->getTracks()) {
    if (track && track->isTempoTrack()) {
      QMessageBox::information(this, "Tempo Track Exists",
                               "A tempo track already exists in this sequence.");
      return;
    }
  }
  
  // Add a new track with instrument 0 (Acoustic Grand Piano - won't be used)
  NoteNagaTrack *track = seq->addTrack(0);
  if (track) {
    track->setTempoTrack(true);
    track->setName("Tempo Track");
    // Move to beginning
    int trackIdx = -1;
    auto tracks = seq->getTracks();
    for (int i = 0; i < tracks.size(); ++i) {
      if (tracks[i] == track) {
        trackIdx = i;
        break;
      }
    }
    if (trackIdx > 0) {
      seq->moveTrack(trackIdx, 0);
    }
  }
}

void TrackListWidget::onRemoveTrack() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
    return;
  }
  
  // Prevent removing tracks during playback
  if (engine->isPlaying()) {
    QMessageBox::warning(this, "Playback Active",
                         "Cannot remove tracks during playback. Please stop playback first.");
    return;
  }

  if (selected_row < 0 || selected_row >= (int)track_widgets.size())
    return;

  // Prevent removing the last track
  if (seq->getTracks().size() <= 1) {
    QMessageBox::warning(this, "Cannot Remove Track",
                         "At least one track must remain in the project.");
    return;
  }

  // Ask for confirmation
  if (QMessageBox::question(this, "Remove Track",
                            "Are you sure you want to remove this track?",
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) != QMessageBox::Yes) {
    return;
  }

  seq->removeTrack(selected_row);
}

void TrackListWidget::onClearTracks() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to clear tracks.");
    return;
  }
  
  // Prevent clearing tracks during playback
  if (engine->isPlaying()) {
    QMessageBox::warning(this, "Playback Active",
                         "Cannot clear tracks during playback. Please stop playback first.");
    return;
  }

  if (QMessageBox::question(this, "Clear All Tracks",
                            "Are you sure you want to remove all tracks? A new empty track will be created.",
                            QMessageBox::Yes | QMessageBox::No,
                            QMessageBox::No) == QMessageBox::Yes) {
    seq->clear();
    
    // Create one empty track so the project is never without tracks
    seq->addTrack(0);  // Piano as default
  }
}

void TrackListWidget::onReloadTracks() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
    return;
  }

  if (QMessageBox::question(this, "Reload Tracks",
                            "Are you sure you want to reload all tracks?") ==
      QMessageBox::Yes) {
    seq->loadFromMidi(seq->getFilePath());
  }
}

void TrackListWidget::onReloadAllSoundFonts() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, tr("No Active Sequence"),
                         tr("Please load a MIDI file first."));
    return;
  }

  // Open file dialog to select SoundFont
  QString sfPath = QFileDialog::getOpenFileName(
      this,
      tr("Select SoundFont for All Tracks"),
      QString(),
      tr("SoundFont Files (*.sf2 *.sf3 *.SF2 *.SF3);;All Files (*)")
  );

  if (sfPath.isEmpty()) {
    return; // User cancelled
  }

  int updatedCount = 0;
  int failedCount = 0;

  for (auto *track : seq->getTracks()) {
    if (!track) continue;
    
    NoteNagaSynthFluidSynth *fluidSynth = 
        dynamic_cast<NoteNagaSynthFluidSynth*>(track->getSynth());
    if (!fluidSynth) continue;
    
    if (fluidSynth->setSoundFont(sfPath.toStdString())) {
      updatedCount++;
    } else {
      failedCount++;
    }
  }

  QString msg = tr("Updated SoundFont on %1 track(s).").arg(updatedCount);
  if (failedCount > 0) {
    msg += tr("\n%1 track(s) failed to update.").arg(failedCount);
  }
  QMessageBox::information(this, tr("SoundFont Update"), msg);
}

void TrackListWidget::showTrackContextMenu(TrackWidget *trackWidget, const QPoint &globalPos) {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) return;
  
  NoteNagaTrack *track = trackWidget->getTrack();
  if (!track) return;
  
  // Find the index of this track
  int trackIdx = -1;
  for (size_t i = 0; i < track_widgets.size(); ++i) {
    if (track_widgets[i] == trackWidget) {
      trackIdx = static_cast<int>(i);
      break;
    }
  }
  
  // Select this track
  if (trackIdx >= 0) {
    updateSelection(seq, trackIdx);
  }
  
  QMenu menu(this);
  
  // Rename track
  QAction *renameAction = menu.addAction(QIcon(":/icons/settings.svg"), "Rename Track");
  connect(renameAction, &QAction::triggered, this, [this, track]() {
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Track", "Track name:",
                                            QLineEdit::Normal, 
                                            QString::fromStdString(track->getName()), &ok);
    if (ok && !newName.isEmpty()) {
      track->setName(newName.toStdString());
    }
  });
  
  // Change instrument
  QAction *instrumentAction = menu.addAction(QIcon(":/icons/midi.svg"), "Change Instrument...");
  connect(instrumentAction, &QAction::triggered, this, [this, track]() {
    InstrumentSelectorDialog dlg(this, GM_INSTRUMENTS, instrument_icon, track->getInstrument());
    if (dlg.exec() == QDialog::Accepted) {
      int gm_index = dlg.getSelectedGMIndex();
      track->setInstrument(gm_index);
    }
  });
  
  menu.addSeparator();
  
  // Toggle visibility
  QAction *visibilityAction = menu.addAction(
    track->isVisible() ? QIcon(":/icons/eye-not-visible.svg") : QIcon(":/icons/eye-visible.svg"),
    track->isVisible() ? "Hide Track" : "Show Track"
  );
  connect(visibilityAction, &QAction::triggered, this, [track]() {
    track->setVisible(!track->isVisible());
  });
  
  // Toggle mute
  QAction *muteAction = menu.addAction(
    track->isMuted() ? QIcon(":/icons/sound-on.svg") : QIcon(":/icons/sound-off.svg"),
    track->isMuted() ? "Unmute Track" : "Mute Track"
  );
  connect(muteAction, &QAction::triggered, this, [this, track]() {
    engine->muteTrack(track, !track->isMuted());
  });
  
  // Toggle solo
  QAction *soloAction = menu.addAction(QIcon(":/icons/solo.svg"), 
                                       track->isSolo() ? "Unsolo Track" : "Solo Track");
  connect(soloAction, &QAction::triggered, this, [this, track]() {
    engine->soloTrack(track, !track->isSolo());
  });
  
  menu.addSeparator();
  
  // Tempo track options
  bool hasTempoTrack = seq->hasTempoTrack();
  if (!hasTempoTrack) {
    QAction *createTempoTrackAction = menu.addAction(QIcon(":/icons/tempo.svg"), "Create Tempo Track");
    connect(createTempoTrackAction, &QAction::triggered, this, [this, seq]() {
      seq->createTempoTrack();
      QMessageBox::information(this, "Tempo Track Created", 
                               "Tempo track has been created. You can now edit tempo changes in the Note Property Editor.");
    });
    
    // Option to set current track as tempo track (for empty tracks)
    if (track->getNotes().empty()) {
      QAction *setAsTempoTrackAction = menu.addAction(QIcon(":/icons/tempo.svg"), "Set as Tempo Track");
      connect(setAsTempoTrackAction, &QAction::triggered, this, [this, seq, track]() {
        seq->setTempoTrack(track);
        QMessageBox::information(this, "Tempo Track Set", 
                                 "This track is now the tempo track. You can edit tempo changes in the Note Property Editor.");
      });
    }
  } else {
    // If this is the tempo track, show tempo-specific options
    if (track->isTempoTrack()) {
      // Toggle active state
      QAction *toggleActiveAction = menu.addAction(
        track->isTempoTrackActive() ? "Deactivate Tempo Track" : "Activate Tempo Track"
      );
      connect(toggleActiveAction, &QAction::triggered, this, [track]() {
        track->setTempoTrackActive(!track->isTempoTrackActive());
      });
      
      // Clear all tempo events
      QAction *clearEventsAction = menu.addAction(QIcon(":/icons/remove.svg"), "Clear Tempo Events...");
      connect(clearEventsAction, &QAction::triggered, this, [this, track]() {
        if (QMessageBox::question(this, "Clear Tempo Events", 
                                  "This will remove all tempo events and reset to default 120 BPM. Continue?",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
          track->resetTempoEvents();
        }
      });
      
      menu.addSeparator();
      
      // Remove tempo track designation
      QAction *removeTempoTrackAction = menu.addAction(QIcon(":/icons/tempo.svg"), "Remove Tempo Track");
      connect(removeTempoTrackAction, &QAction::triggered, this, [this, seq]() {
        if (QMessageBox::question(this, "Remove Tempo Track", 
                                  "This will remove the tempo track designation and use fixed tempo. Continue?",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
          seq->removeTempoTrack();
        }
      });
    }
  }
  
  menu.addSeparator();
  
  // Duplicate track
  QAction *duplicateAction = menu.addAction("Duplicate Track");
  duplicateAction->setEnabled(!track->isTempoTrack());  // Can't duplicate tempo track
  connect(duplicateAction, &QAction::triggered, this, &TrackListWidget::onDuplicateTrack);
  
  // Create new sequence with this track
  if (!track->isTempoTrack()) {
    QAction *createSeqAction = menu.addAction(QIcon(":/icons/add.svg"), "Create New Sequence with This Track...");
    connect(createSeqAction, &QAction::triggered, this, [this, seq, track]() {
      bool ok;
      QString seqName = QInputDialog::getText(this, tr("New Sequence"), 
                                              tr("Sequence name:"),
                                              QLineEdit::Normal, 
                                              QString::fromStdString(track->getName()) + " Seq", &ok);
      if (!ok || seqName.isEmpty()) return;
      
      // Create new sequence with tempo from original sequence
      NoteNagaMidiSeq *newSeq = new NoteNagaMidiSeq();
      newSeq->setTempo(seq->getTempo());
      newSeq->setPPQ(seq->getPPQ());
      
      // Add to runtime data
      engine->getRuntimeData()->addSequence(newSeq);
      
      // Copy track to new sequence
      NoteNagaTrack *newTrack = newSeq->addTrack(track->getInstrument().value_or(0));
      if (newTrack) {
        // Use the entered sequence name for the track
        newTrack->setName(seqName.toStdString());
        newTrack->setVisible(track->isVisible());
        newTrack->setColor(track->getColor());
        newTrack->setChannel(track->getChannel());
        
        // Copy notes
        for (const auto &note : track->getNotes()) {
          newTrack->addNote(note);
        }
      }
      
      // Recalculate max tick for proper duration
      newSeq->computeMaxTick();
      
      // Ask if user wants to switch to the new sequence
      if (QMessageBox::question(this, tr("Sequence Created"), 
                                tr("New sequence has been created with track '%1'.\n\nSwitch to it now?").arg(seqName),
                                QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        engine->getRuntimeData()->setActiveSequence(newSeq);
      }
      
      emit newSequenceCreated(newSeq);
    });
  }
  
  menu.addSeparator();
  
  // Move up/down
  QAction *moveUpAction = menu.addAction("Move Up");
  moveUpAction->setEnabled(trackIdx > 0);
  connect(moveUpAction, &QAction::triggered, this, &TrackListWidget::onMoveTrackUp);
  
  QAction *moveDownAction = menu.addAction(QIcon(":/icons/arrow-down.svg"), "Move Down");
  moveDownAction->setEnabled(trackIdx >= 0 && trackIdx < (int)track_widgets.size() - 1);
  connect(moveDownAction, &QAction::triggered, this, &TrackListWidget::onMoveTrackDown);
  
  menu.addSeparator();
  
  // Remove track
  QAction *removeAction = menu.addAction(QIcon(":/icons/remove.svg"), "Remove Track");
  connect(removeAction, &QAction::triggered, this, &TrackListWidget::onRemoveTrack);
  
  menu.exec(globalPos);
}

void TrackListWidget::onDuplicateTrack() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) return;
  
  if (selected_row < 0 || selected_row >= (int)track_widgets.size()) return;
  
  NoteNagaTrack *sourceTrack = seq->getTracks()[selected_row];
  if (!sourceTrack) return;
  
  // Create new track with same instrument
  NoteNagaTrack *newTrack = seq->addTrack(sourceTrack->getInstrument().value_or(0));
  if (!newTrack) return;
  
  // Copy properties
  newTrack->setName(sourceTrack->getName() + " (Copy)");
  newTrack->setColor(sourceTrack->getColor());
  
  // Copy notes with proper parent assignment
  for (const auto &note : sourceTrack->getNotes()) {
    NN_Note_t newNote = note;
    newNote.id = nn_generate_unique_note_id();
    newNote.parent = newTrack;
    newTrack->addNote(newNote);
  }
}

void TrackListWidget::onMoveTrackUp() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) return;
  
  if (selected_row <= 0 || selected_row >= (int)seq->getTracks().size()) return;
  
  seq->moveTrack(selected_row, selected_row - 1);
  selected_row--;
}

void TrackListWidget::onMoveTrackDown() {
  NoteNagaMidiSeq *seq = engine->getRuntimeData()->getActiveSequence();
  if (!seq) return;
  
  if (selected_row < 0 || selected_row >= (int)seq->getTracks().size() - 1) return;
  
  seq->moveTrack(selected_row, selected_row + 1);
  selected_row++;
}

void TrackListWidget::updateTrackMeters() {
  auto* dspEngine = engine->getDSPEngine();
  if (!dspEngine) return;
  
  for (TrackWidget* widget : track_widgets) {
    if (!widget) continue;
    NoteNagaTrack* track = widget->getTrack();
    if (!track) continue;
    
    auto rms = dspEngine->getTrackVolumeDb(track);
    if (TrackStereoMeter* meter = widget->getStereoMeter()) {
      meter->setVolumesDb(rms.first, rms.second);
    }
  }
}