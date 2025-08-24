#include "track_list_widget.h"

#include "../dialogs/instrument_selector_dialog.h"
#include "../nn_gui_utils.h"
#include <QMessageBox>
#include <QMouseEvent>

TrackListWidget::TrackListWidget(NoteNagaEngine *engine_, QWidget *parent)
    : QWidget(parent), engine(engine_), selected_row(-1) {
  this->title_widget = nullptr;
  initTitleUI();
  initUI();

  NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
  reloadTracks(seq);

  // Signals
  connect(engine->getProject(), &NoteNagaProject::activeSequenceChanged, this,
          &TrackListWidget::reloadTracks);
  connect(engine->getProject(),
          &NoteNagaProject::activeSequenceTrackListChanged, this,
          &TrackListWidget::reloadTracks);
  connect(engine->getMixer(), &NoteNagaMixer::noteInSignal, this,
          &TrackListWidget::handlePlayingNote);
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

  QPushButton *btn_remove = create_small_button(
      ":/icons/remove.svg", "Remove selected Track", "RemoveButton");

  QPushButton *btn_clear = create_small_button(
      ":/icons/clear.svg", "Clear all Tracks", "ClearButton");

  QPushButton *btn_reload = create_small_button(
      ":/icons/reload.svg", "Reload Tracks from MIDI", "ReloadButton");

  layout->addWidget(btn_add, 0, Qt::AlignRight);
  layout->addWidget(btn_remove, 0, Qt::AlignRight);
  layout->addWidget(btn_clear, 0, Qt::AlignRight);
  layout->addWidget(btn_reload, 0, Qt::AlignRight);

  connect(btn_add, &QPushButton::clicked, this, &TrackListWidget::onAddTrack);
  connect(btn_remove, &QPushButton::clicked, this,
          &TrackListWidget::onRemoveTrack);
  connect(btn_clear, &QPushButton::clicked, this,
          &TrackListWidget::onClearTracks);
  connect(btn_reload, &QPushButton::clicked, this,
          &TrackListWidget::onReloadTracks);
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

    // Custom mousePressEvent via subclass or signal (see below)
    connect(widget, &TrackWidget::clicked, this, [this, seq, idx]() {
      updateSelection(seq, static_cast<int>(idx));
    });

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

void TrackListWidget::handlePlayingNote(const NN_Note_t &note) {
  NoteNagaTrack *track = note.parent;
  if (!track)
    return;
  NoteNagaProject *project = engine->getProject();

  double time_ms = note_time_ms(note, project->getPPQ(), project->getTempo());
  for (auto *w : track_widgets) {
    if (w->getTrack() == track && note.velocity.has_value() &&
        note.velocity.value() > 0) {
      w->getVolumeBar()->setValue(static_cast<double>(note.velocity.value()),
                                  time_ms);
      break;
    }
  }
}

void TrackListWidget::onAddTrack() {
  NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
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

void TrackListWidget::onRemoveTrack() {
  NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
    return;
  }

  if (selected_row < 0 || selected_row >= (int)track_widgets.size())
    return;
  seq->removeTrack(selected_row);
}

void TrackListWidget::onClearTracks() {
  NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to clear tracks.");
    return;
  }

  if (QMessageBox::question(this, "Clear All Tracks",
                            "Are you sure you want to remove all tracks?") ==
      QMessageBox::Yes) {
    seq->clear();
    engine->getMixer()->clearRoutingTable();
  }
}

void TrackListWidget::onReloadTracks() {
  NoteNagaMidiSeq *seq = engine->getProject()->getActiveSequence();
  if (!seq) {
    QMessageBox::warning(this, "No Active Sequence",
                         "Please load a MIDI file first to add tracks.");
    return;
  }

  if (QMessageBox::question(this, "Reload Tracks",
                            "Are you sure you want to reload all tracks?") ==
      QMessageBox::Yes) {
    seq->loadFromMidi(seq->getFilePath());
    engine->getMixer()->createDefaultRouting();
  }
}