#ifndef KEYBOARDSTREAM_HPP
#define KEYBOARDSTREAM_HPP

#include <chrono> // for std::chrono::seconds
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "effect.hpp"
#include "note.hpp"
#include "notes.hpp"
#include "sound.hpp"

#include "json.hpp"

static constexpr int SAMPLERATE = 44100;

class KeyboardStream {
public:
  KeyboardStream(int bufferSize, int sampleRate)
      : bufferSize(bufferSize), sampleRate(sampleRate) {}
  ~KeyboardStream() { teardown(); }

  void setup() {}

  void teardown() {}

  void setVolume(float volume_) { this->volume = volume_; }

  void changeOctave(int delta) {
    // Prevent going higher if there's a note with octave 8
    if (delta > 0) {
      for (const auto &kv : keyPressToNote) {
        if (kv.second.back() == '8') {
          return; // We have an octave 8, can't increase further
        }
      }
    }

    // Prevent going lower if there's a note with octave 1
    if (delta < 0) {
      for (const auto &kv : keyPressToNote) {
        if (kv.second.back() == '0') {
          return; // We have an octave 1, can't decrease further
        }
      }
    }

    // Modify the octaves
    for (auto &kv : keyPressToNote) {
      std::string note = kv.second;
      int currentOctave = note.back() - '0'; // Convert char to int
      currentOctave += delta;
      note.back() = currentOctave + '0'; // Convert int back to char
      kv.second = note;
    }
  }

  void setLoaderFunc(void (*func)(unsigned, unsigned)) {
    this->loaderFunc = func;
  }

  void prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f,
                    std::vector<Effect> &effects, int nbrThreads);
  void prepareSound(int sampleRate, ADSR &adsr, Sound::Rank::Preset preset,
                    std::vector<Effect> &effects, int nbrThreads);
  void fillBuffer(short *buffer, const int len);
  void registerNote(const std::string &note);
  void registerNoteRelease(const std::string &note);
  void registerButtonPress(int note);
  void registerButtonRelease(int note);
  void printInstructions();

  void loadSoundMap(std::string soundMapFile) {
    // Open the file for reading
    std::ifstream file(soundMapFile);
    this->soundMapFile = soundMapFile;
    if (!file.is_open()) {
      std::cerr << "Failed to open sound map file: " << soundMapFile
                << std::endl;
      return;
    }

    // Parse the JSON content
    nlohmann::json j;
    file >> j;

    // Map the content to soundMap
    for (const auto &[key, value] : j.items()) {
      soundMap[key] = value.get<std::string>();
    }
  }

  static long currentTimeMillis() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return static_cast<long>(millis);
  }

  float generateSample(std::string note, float phase);

  class Synth {
  public:
    float volume;
    int octave;
    int detune;
    ADSR adsr;
    Sound::Rank::Preset sound;
    int sampleRate;

    void setVolume(float volume);
    void setOctave(int octave);
    void setDetune(int detune);
    void setADSR(ADSR adsr);
    void setSound(Sound::Rank::Preset sound);

    void initialize(int sampleRate);

    float getSample(const std::string &note);
    void reset(const std::string &note);

  private:
    void updateFrequencies() {
      for (auto &kv : this->ranks) {
        Sound::Rank r = kv.second;
        for (Sound::Pipe &pipe : r.pipes) {
          Note note = pipe.first;
          note.frequencyAltered = note.frequency * 2 *
                                  pow(2, this->detune / 1200.0) * 2 *
                                  pow(2, this->octave);
        }
      }
    }
    int index = 0;
    std::map<std::string, Sound::Rank> ranks;
    bool initialized = false;
  };

  struct NotePress {
    ADSR adsr;
    std::string note;
    long time;
    float phase = 0;
    bool release = false;
    int index = 0;
  };

private:
  std::map<std::string, std::string> soundMap;
  void (*loaderFunc)(unsigned, unsigned) = nullptr;
  std::string soundMapFile;
  int bufferSize;
  int sampleRate;
  std::mutex mtx;

  std::map<std::string, int> keyToBufferIndex;
  std::map<std::string, Note> notes;
  std::map<std::string, NotePress> notesPressed;
  std::thread keyboardPressWatchdog;

  short amplitude = 32767;
  float duration = 0.1f;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(SAMPLERATE *duration));
  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Sound::Rank::Preset rankPreset = Sound::Rank::Preset::None;
  std::map<std::string, Sound::Rank> ranks;

  float volume = 1.0;

  std::map<int, std::string> keyPressToNote = {
      {static_cast<int>('1'), "C5"}, {static_cast<int>('2'), "D5"},
      {static_cast<int>('3'), "E5"}, {static_cast<int>('4'), "F5"},
      {static_cast<int>('5'), "G5"}, {static_cast<int>('6'), "A5"},
      {static_cast<int>('7'), "B5"}, {static_cast<int>('8'), "C6"},
      {static_cast<int>('9'), "D6"}, {static_cast<int>('0'), "E6"},

      {static_cast<int>('q'), "C4"}, {static_cast<int>('w'), "D4"},
      {static_cast<int>('e'), "E4"}, {static_cast<int>('r'), "F4"},
      {static_cast<int>('t'), "G4"}, {static_cast<int>('y'), "A4"},
      {static_cast<int>('u'), "B4"}, {static_cast<int>('i'), "C5"},

      {static_cast<int>('a'), "C3"}, {static_cast<int>('s'), "D3"},
      {static_cast<int>('d'), "E3"}, {static_cast<int>('f'), "F3"},
      {static_cast<int>('g'), "G3"}, {static_cast<int>('h'), "A3"},
      {static_cast<int>('j'), "B3"}, {static_cast<int>('k'), "C4"},
      {static_cast<int>('l'), "D4"},

      {static_cast<int>('z'), "C2"}, {static_cast<int>('x'), "D2"},
      {static_cast<int>('c'), "E2"}, {static_cast<int>('v'), "F2"},
      {static_cast<int>('b'), "G2"}, {static_cast<int>('n'), "A2"},
      {static_cast<int>('m'), "B2"}};

  std::map<int, std::function<void()>> keyPressToAction = {
      {static_cast<int>('o'), [this]() { this->changeOctave(-1); }},
      {static_cast<int>('p'), [this]() { this->changeOctave(1); }}};
};
#endif
