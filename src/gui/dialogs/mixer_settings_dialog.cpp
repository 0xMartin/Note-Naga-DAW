#include "mixer_settings_dialog.h"

#include <note_naga_engine/logger.h>

#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

MixerSettingsDialog::MixerSettingsDialog(NoteNagaEngine *engine,
                                         QWidget *parent)
    : QDialog(parent), engine(engine) {
  setWindowTitle("Mixer Settings");
  setMinimumSize(500, 400);

  setupUI();
  populateSynthesizerList();
  updateConfigurationVisibility();
}

MixerSettingsDialog::~MixerSettingsDialog() {
  // Nothing to clean up specifically
}

void MixerSettingsDialog::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  // Synthesizer list section
  QGroupBox *synthListGroup = new QGroupBox("Synthesizers", this);
  QVBoxLayout *synthListLayout = new QVBoxLayout(synthListGroup);

  synthListWidget = new QListWidget(synthListGroup);
  synthListLayout->addWidget(synthListWidget);

  // Add/remove synthesizer controls
  QHBoxLayout *synthControlsLayout = new QHBoxLayout();

  QLabel *synthTypeLabel = new QLabel("Type:", synthListGroup);
  synthTypeComboBox = new QComboBox(synthListGroup);
  synthTypeComboBox->addItem("FluidSynth", "fluidsynth");
  synthTypeComboBox->addItem("External MIDI", "external_midi");

  addButton = new QPushButton("Add", synthListGroup);
  removeButton = new QPushButton("Remove", synthListGroup);

  synthControlsLayout->addWidget(synthTypeLabel);
  synthControlsLayout->addWidget(synthTypeComboBox);
  synthControlsLayout->addStretch();
  synthControlsLayout->addWidget(addButton);
  synthControlsLayout->addWidget(removeButton);

  synthListLayout->addLayout(synthControlsLayout);
  mainLayout->addWidget(synthListGroup);

  // FluidSynth Configuration
  fluidSynthGroup = new QGroupBox("FluidSynth Configuration", this);
  QFormLayout *fluidSynthLayout = new QFormLayout(fluidSynthGroup);

  soundFontPathEdit = new QLineEdit(fluidSynthGroup);
  browseSoundFontButton = new QPushButton("Browse...", fluidSynthGroup);

  QHBoxLayout *soundFontLayout = new QHBoxLayout();
  soundFontLayout->addWidget(soundFontPathEdit);
  soundFontLayout->addWidget(browseSoundFontButton);

  fluidSynthLayout->addRow("SoundFont:", soundFontLayout);

  applyFluidSynthButton = new QPushButton("Apply Changes", fluidSynthGroup);
  fluidSynthLayout->addRow("", applyFluidSynthButton);

  mainLayout->addWidget(fluidSynthGroup);

  // External MIDI Configuration
  externalMidiGroup = new QGroupBox("External MIDI Configuration", this);
  QFormLayout *externalMidiLayout = new QFormLayout(externalMidiGroup);

  midiPortComboBox = new QComboBox(externalMidiGroup);
  refreshPortsButton = new QPushButton("Refresh", externalMidiGroup);

  QHBoxLayout *midiPortLayout = new QHBoxLayout();
  midiPortLayout->addWidget(midiPortComboBox);
  midiPortLayout->addWidget(refreshPortsButton);

  externalMidiLayout->addRow("MIDI Port:", midiPortLayout);

  applyExternalMidiButton = new QPushButton("Apply Changes", externalMidiGroup);
  externalMidiLayout->addRow("", applyExternalMidiButton);

  mainLayout->addWidget(externalMidiGroup);

  // Populate MIDI ports initially
  refreshMidiPorts();

  // Close button
  closeButton = new QPushButton("Close", this);
  QHBoxLayout *bottomLayout = new QHBoxLayout();
  bottomLayout->addStretch();
  bottomLayout->addWidget(closeButton);
  mainLayout->addLayout(bottomLayout);

  // Connect signals and slots
  connect(addButton, &QPushButton::clicked, this,
          &MixerSettingsDialog::addSynthesizer);
  connect(removeButton, &QPushButton::clicked, this,
          &MixerSettingsDialog::removeSynthesizer);
  connect(synthListWidget, &QListWidget::currentRowChanged, this,
          &MixerSettingsDialog::onSynthesizerSelected);
  connect(browseSoundFontButton, &QPushButton::clicked, this,
          &MixerSettingsDialog::browseSoundFont);
  connect(refreshPortsButton, &QPushButton::clicked, this,
          &MixerSettingsDialog::refreshMidiPorts);
  connect(applyFluidSynthButton, &QPushButton::clicked, this,
          &MixerSettingsDialog::applyFluidSynthChanges);
  connect(applyExternalMidiButton, &QPushButton::clicked, this,
          &MixerSettingsDialog::applyExternalMidiChanges);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void MixerSettingsDialog::populateSynthesizerList() {
  synthListWidget->clear();

  auto synthesizers = engine->getSynthesizers();
  for (auto synth : synthesizers) {
    QString displayName = QString::fromStdString(synth->getName());

    QListWidgetItem *item = new QListWidgetItem(displayName);
    item->setData(Qt::UserRole,
                  QVariant::fromValue(static_cast<void *>(synth)));
    synthListWidget->addItem(item);
  }

  // Disable remove button if no synthesizers are available
  removeButton->setEnabled(synthListWidget->count() > 0);
}

void MixerSettingsDialog::onSynthesizerSelected(int index) {
  if (index < 0) {
    fluidSynthGroup->setVisible(false);
    externalMidiGroup->setVisible(false);
    return;
  }

  QListWidgetItem *item = synthListWidget->item(index);
  if (!item)
    return;

  NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer *>(
      item->data(Qt::UserRole).value<void *>());

  if (NoteNagaSynthFluidSynth *fluidSynth =
          dynamic_cast<NoteNagaSynthFluidSynth *>(synth)) {
    // Show FluidSynth configuration
    fluidSynthGroup->setVisible(true);
    externalMidiGroup->setVisible(false);

    // Display current SoundFont path
    soundFontPathEdit->setText(
        QString::fromStdString(fluidSynth->getSoundFontPath()));
  } else if (NoteNagaSynthExternalMidi *externalMidi =
                 dynamic_cast<NoteNagaSynthExternalMidi *>(synth)) {
    // Show External MIDI configuration
    fluidSynthGroup->setVisible(false);
    externalMidiGroup->setVisible(true);

    // Select the current MIDI port if available
    QString currentPort =
        QString::fromStdString(externalMidi->getCurrentPortName());
    int portIndex = midiPortComboBox->findText(currentPort);
    if (portIndex >= 0) {
      midiPortComboBox->setCurrentIndex(portIndex);
    }
  } else {
    // Unknown synthesizer type
    fluidSynthGroup->setVisible(false);
    externalMidiGroup->setVisible(false);
  }
}

void MixerSettingsDialog::updateConfigurationVisibility() {
  // Initially hide configuration panels until a synthesizer is selected
  fluidSynthGroup->setVisible(false);
  externalMidiGroup->setVisible(false);

  // Show appropriate panel if a synthesizer is selected
  int currentIndex = synthListWidget->currentRow();
  if (currentIndex >= 0) {
    onSynthesizerSelected(currentIndex);
  }
}

void MixerSettingsDialog::addSynthesizer() {
  QString synthName;
  QString type = synthTypeComboBox->currentData().toString();

  // Create a name for the new synthesizer
  if (type == "fluidsynth") {
    synthName = "FluidSynth";
  } else if (type == "external_midi") {
    synthName = "External MIDI";
  } else {
    return; // Unknown type
  }

  // Add a number suffix if name already exists
  int suffix = 1;
  bool nameExists;
  QString finalName;

  do {
    nameExists = false;
    finalName =
        suffix == 1 ? synthName : QString("%1 %2").arg(synthName).arg(suffix);

    for (int i = 0; i < synthListWidget->count(); ++i) {
      if (synthListWidget->item(i)->text().startsWith(finalName)) {
        nameExists = true;
        break;
      }
    }

    if (nameExists) {
      suffix++;
    }
  } while (nameExists);

  // Create and add the synthesizer
  NoteNagaSynthesizer *newSynth = nullptr;

  try {
    if (type == "fluidsynth") {
      // Create default FluidSynth with empty SoundFont path
      // User will need to set the SoundFont path later
      newSynth = new NoteNagaSynthFluidSynth(finalName.toStdString(), "");
    } else if (type == "external_midi") {
      // Create external MIDI synthesizer with auto-selected port
      newSynth = new NoteNagaSynthExternalMidi(finalName.toStdString());
    }

    if (newSynth) {
      engine->addSynthesizer(newSynth);
      populateSynthesizerList();

      // Select the newly added synthesizer
      for (int i = 0; i < synthListWidget->count(); ++i) {
        QListWidgetItem *item = synthListWidget->item(i);
        if (static_cast<NoteNagaSynthesizer *>(
                item->data(Qt::UserRole).value<void *>()) == newSynth) {
          synthListWidget->setCurrentRow(i);
          break;
        }
      }
    }
  } catch (const std::exception &e) {
    QMessageBox::critical(
        this, "Error",
        QString("Failed to create synthesizer: %1").arg(e.what()));
  }
}

void MixerSettingsDialog::removeSynthesizer() {
  int currentIndex = synthListWidget->currentRow();
  if (currentIndex < 0)
    return;

  QListWidgetItem *item = synthListWidget->item(currentIndex);
  if (!item)
    return;

  NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer *>(
      item->data(Qt::UserRole).value<void *>());

  // Confirm removal
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, "Confirm Removal",
      QString("Are you sure you want to remove synthesizer '%1'?")
          .arg(item->text()),
      QMessageBox::Yes | QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    engine->removeSynthesizer(synth);

    // Note: The engine will take care of deleting the synthesizer object
    // We just need to update our UI
    populateSynthesizerList();
    updateConfigurationVisibility();
  }
}

void MixerSettingsDialog::browseSoundFont() {
  QString fileName = QFileDialog::getOpenFileName(
      this, "Select SoundFont File",
      "", // Default directory
      "SoundFont Files (*.sf2 *.sf3);;All Files (*)");

  if (!fileName.isEmpty()) {
    soundFontPathEdit->setText(fileName);
  }
}

void MixerSettingsDialog::refreshMidiPorts() {
  midiPortComboBox->clear();

  // Get available MIDI ports
  std::vector<std::string> ports =
      NoteNagaSynthExternalMidi::getAvailableMidiOutputPorts();

  for (const auto &port : ports) {
    midiPortComboBox->addItem(QString::fromStdString(port));
  }

  // Disable the combobox if no ports are available
  midiPortComboBox->setEnabled(!ports.empty());
  if (ports.empty()) {
    midiPortComboBox->addItem("No MIDI ports available");
  }
}

void MixerSettingsDialog::applyFluidSynthChanges() {
  int currentIndex = synthListWidget->currentRow();
  if (currentIndex < 0)
    return;

  QListWidgetItem *item = synthListWidget->item(currentIndex);
  if (!item)
    return;

  NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer *>(
      item->data(Qt::UserRole).value<void *>());
  NoteNagaSynthFluidSynth *fluidSynth =
      dynamic_cast<NoteNagaSynthFluidSynth *>(synth);

  if (!fluidSynth) {
    QMessageBox::warning(this, "Error",
                         "Selected synthesizer is not a FluidSynth instance.");
    return;
  }

  QString soundFontPath = soundFontPathEdit->text();
  if (soundFontPath.isEmpty()) {
    QMessageBox::warning(this, "Error", "SoundFont path cannot be empty.");
    return;
  }

  // Apply SoundFont change in real-time
  bool success = fluidSynth->setSoundFont(soundFontPath.toStdString());

  if (success) {
    QMessageBox::information(this, "SoundFont Changed",
                             "SoundFont successfully changed to: " +
                                 soundFontPath);

    // Update the display name to show the current state
    item->setText(QString::fromStdString(synth->getName()) + " (FluidSynth)");
  } else {
    QMessageBox::warning(
        this, "SoundFont Change Failed",
        "Failed to load SoundFont: " + soundFontPath +
            "\n\n"
            "Please check if the file exists and is a valid SoundFont file.");
  }
}

void MixerSettingsDialog::applyExternalMidiChanges() {
  int currentIndex = synthListWidget->currentRow();
  if (currentIndex < 0)
    return;

  QListWidgetItem *item = synthListWidget->item(currentIndex);
  if (!item)
    return;

  NoteNagaSynthesizer *synth = static_cast<NoteNagaSynthesizer *>(
      item->data(Qt::UserRole).value<void *>());
  NoteNagaSynthExternalMidi *externalMidi =
      dynamic_cast<NoteNagaSynthExternalMidi *>(synth);

  if (!externalMidi) {
    QMessageBox::warning(
        this, "Error",
        "Selected synthesizer is not an External MIDI instance.");
    return;
  }

  if (midiPortComboBox->count() == 0 || midiPortComboBox->currentIndex() < 0) {
    QMessageBox::warning(this, "Error", "No MIDI port selected.");
    return;
  }

  QString selectedPort = midiPortComboBox->currentText();

  // Apply the MIDI port change
  bool success = externalMidi->setMidiOutputPort(selectedPort.toStdString());

  if (success) {
    QMessageBox::information(this, "Success",
                             "MIDI port updated successfully.");
  } else {
    QMessageBox::warning(
        this, "Error",
        "Failed to set MIDI port. Please check if the device is connected.");
  }
}