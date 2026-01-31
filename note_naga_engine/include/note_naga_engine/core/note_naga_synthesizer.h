#pragma once

#include <cstddef>
#include <note_naga_engine/core/async_queue_component.h>
#include <note_naga_engine/core/types.h>
#include <string>

/*******************************************************************************************************/
// Synthesizer Queue Message
/*******************************************************************************************************/

/**
 * @brief Represents a message for the Note Naga Synthesizer queue.
 */
struct NOTE_NAGA_ENGINE_API NN_SynthMessage_t {
  NN_Note_t note; /// <MIDI note to play or stop>
  int channel;    ///< MIDI channel to use
  bool play;      /// <True to play note, false to stop>
  float pan;      /// <Stereo pan value (-1.0 left, 1.0 right)>
};

/*******************************************************************************************************/
// Synthesizer Base Class
/*******************************************************************************************************/

/**
 * Abstract base class for Note Naga Synthesizers.
 * This class defines the interface for synthesizers that can play MIDI notes.
 */
#ifndef QT_DEACTIVATED
class NOTE_NAGA_ENGINE_API NoteNagaSynthesizer
    : public QObject,
      public AsyncQueueComponent<NN_SynthMessage_t, 1024> {
  Q_OBJECT
#else
class NOTE_NAGA_ENGINE_API NoteNagaSynthesizer
    : public AsyncQueueComponent<NN_SynthMessage_t, 1024> {
#endif
public:
#ifndef QT_DEACTIVATED
  NoteNagaSynthesizer(const std::string &name) : QObject(nullptr), name(name) {}
#else
  NoteNagaSynthesizer(const std::string &name) : name(name) {}
#endif
  virtual ~NoteNagaSynthesizer() = default;

  /**
   * @brief Set the name of the synthesizer.
   * @param name Name of the synthesizer.
   */
  void setName(const std::string &name) { this->name = name; }

  /**
   * @brief Sets a parameter for the synthesizer.
   * @param param Parameter name.
   * @param value Value to set.
   */
  std::string getName() const { return this->name; }

  /**
   * @brief Plays a MIDI note.
   * @param note The note to play.
   * @param channel MIDI channel to use.
   * @param pan Stereo pan value (-1.0 left, 1.0 right).
   */
  virtual void playNote(const NN_Note_t &note, int channel = 0,
                        float pan = 0.0f) = 0;

  /**
   * @brief Stops a MIDI note.
   * @param note The note to stop.
   */
  virtual void stopNote(const NN_Note_t &note) = 0;

  /**
   * @brief Stops all notes for the specified sequence and/or track.
   * @param seq Optional pointer to the MIDI sequence. If is nullptr, stops all
   * notes in all sequences.
   * @param track Optional pointer to the track. If is nullptr, stops all notes
   * in the sequence.
   */
  virtual void stopAllNotes(NoteNagaMidiSeq *seq = nullptr,
                            NoteNagaTrack *track = nullptr) = 0;

  /**
   * @brief Sets the master pan for all channels immediately.
   * @param pan Pan value (-1.0 left, 0.0 center, 1.0 right).
   */
  virtual void setMasterPan(float pan) { (void)pan; }

  /**
   * @brief Get a configuration value for the synthesizer
   * @param key Configuration key
   * @return Configuration value or empty string if not found
   */
  virtual std::string getConfig(const std::string &key) const { return ""; }

  /**
   * @brief Set a configuration value for the synthesizer
   * @param key Configuration key
   * @param value Configuration value
   * @return True if configuration was successfully applied
   */
  virtual bool setConfig(const std::string &key, const std::string &value) {
    return false;
  }

  /**
   * @brief Get a list of supported configuration keys
   * @return Vector of supported configuration keys
   */
  virtual std::vector<std::string> getSupportedConfigKeys() const { return {}; }

protected:
  std::string name; ///< Name of the synthesizer

  // playing notes map: track -> note ID -> playing note
  struct PlayedNote_t {
    NN_Note_t note;
    int channel;
  };
  typedef std::unordered_map<long, PlayedNote_t> TrackNotesMap;
  std::unordered_map<NoteNagaTrack *, TrackNotesMap> playing_notes_;

  // current channel programs
  std::unordered_map<int, int> channel_programs_;

  // curent channel pan values
  std::unordered_map<int, float> channel_pan_;

  /**
   * @brief Called when a new item is dequeued from the component's queue.
   * This method processes the MIDI note play/stop messages.
   * @param value The message to process.
   */
  void onItem(const NN_SynthMessage_t &value) override {
    if (value.play)
      playNote(value.note, value.channel, value.pan);
    else
      stopNote(value.note);
  }

#ifndef QT_DEACTIVATED
Q_SIGNALS:
  /**
   * @brief Signal emitted when a synthesizer is updated.
   */
  void synthUpdated(NoteNagaSynthesizer *synth);
#endif
};

/*******************************************************************************************************/
// Interface for Soft Synthesizers
/*******************************************************************************************************/

class NOTE_NAGA_ENGINE_API INoteNagaSoftSynth {
public:
  virtual void renderAudio(float *left, float *right, size_t num_frames) = 0;
};