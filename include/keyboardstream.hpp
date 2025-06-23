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
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "effect.hpp"
#include "note.hpp"
#include "notes.hpp"
#include "sound.hpp"
#include "waveread.hpp"

#include "json.hpp"

#ifndef SAMPLERATE
#define SAMPLERATE 44100
#endif

class KeyboardStream {
public:
  KeyboardStream(int sampleRate) : sampleRate(sampleRate) {}
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

  void prepareSound(int sampleRate, ADSR &adsr,
                    std::vector<Effect<float>> &effects);
  void fillBuffer(float *buffer, const int len);
  void registerNote(const std::string &note);
  void registerNoteRelease(const std::string &note);
  void registerButtonPress(int note);
  void registerButtonRelease(int note);
  void printInstructions();

  void copyEffectsToSynths() {
    for (int i = 0; i < this->synth.size(); i++) {
      this->synth[i].setEffects(this->effects);
    }
  }

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

  float generateSample(std::string note, float phase, int index);
  struct mutex_holder {
    std::mutex mutex;
    mutex_holder() : mutex() {}
    mutex_holder(mutex_holder const &other) : mutex() {}
  };

  class Oscillator {
  public:
    float volume = 0;
    int octave = 0;
    int detune = 0;
    ADSR adsr;
    Sound::Rank<float>::Preset sound = Sound::Rank<float>::Preset::Sine;
    int sampleRate;

    Oscillator(int sampleRate) {
      this->sampleRate = sampleRate;
      this->initialize();
    }

    // In the future, add effects here too
    nlohmann::json toJson() const {
      return {{"sound", Sound::Rank<float>::presetToJson(this->sound)},
              {"volume", this->volume},
              {"octave", this->octave},
              {"detune", this->detune},
              {"adsr", this->adsr.toJson()},
              {"sampleRate", this->sampleRate}};
    };

    static std::optional<Oscillator> fromJson(const nlohmann::json &j) {
      // Ensure sampleRate exists and is valid
      if (!j.contains("sampleRate") || !j["sampleRate"].is_number_integer())
        return std::nullopt;

      int sampleRate = j["sampleRate"].get<int>();
      Oscillator osc(sampleRate);

      // Parse volume, octave, detune (optional; default to existing values)
      if (j.contains("volume") && j["volume"].is_number())
        osc.volume = j["volume"].get<float>();

      if (j.contains("octave") && j["octave"].is_number_integer())
        osc.octave = j["octave"].get<int>();

      if (j.contains("detune") && j["detune"].is_number_integer())
        osc.detune = j["detune"].get<int>();

      // Parse ADSR
      if (j.contains("adsr")) {
        auto adsrOpt = ADSR::fromJson(j["adsr"]);
        if (!adsrOpt)
          return std::nullopt;
        osc.adsr = *adsrOpt;
      }

      // Parse Sound Preset
      if (j.contains("sound")) {
        auto presetOpt = Sound::Rank<float>::jsonToPreset(j["sound"]);
        if (!presetOpt)
          return std::nullopt;
        osc.sound = *presetOpt;
      }

      osc.initialize();

      return osc;
    }

    void setVolume(float volume);
    void setOctave(int octave);
    void setDetune(int detune);
    void setADSR(ADSR adsr);
    void setSound(Sound::Rank<float>::Preset sound);

    void initialize();

    std::string printSynthConfig() const;

    float getSample(const std::string &note, int index);
    void reset(const std::string &note);
    void updateFrequencies() {
      std::lock_guard<std::mutex> lk(this->ranksMtx.mutex);
      for (auto &kv : this->ranks) {
        Sound::Rank<float> &r = kv.second;
        for (Sound::Pipe &pipe : r.pipes) {
          Note &note = pipe.first;
          note.frequencyAltered = note.frequency * 2 *
                                  pow(2, this->detune / 1200.0) * 2 *
                                  pow(2, this->octave);
        }
      }
    }
    void setSoundMap(std::map<std::string, std::string> &soundMap);
    void setEffects(std::vector<Effect<float>> &effects) {
      this->effects = effects;
      initialize();
    }

    std::vector<Effect<float>> effects;

  private:
    int index = 0;
    mutex_holder ranksMtx;
    std::map<std::string, Sound::Rank<float>> ranks;
    bool initialized = false;
    std::map<std::string, std::vector<short>> samples;
  };

  struct NotePress {
    ADSR adsr;
    std::string note;
    long time;
    double frequency;
    float phase = 0;
    bool release = false;
    int index = 0;
    int rankIndex = 0;

    void debugPrint() const {
      printw("Note: %s | Time: %ld | Freq: %.2f | Release: %s | Index: %d\n",
             note.c_str(), time, frequency, release ? "true" : "false", index);
    }
  };

  std::vector<Oscillator> synth;
  float gain = 0.00001f;

  void printSynthConfig() const;
  void printNotesPressed() const {
    printw("=== Notes Pressed (%zu entries) ===\n", notesPressed.size());
    for (const auto &pair : notesPressed) {
      const std::string &key = pair.first;
      const NotePress &np = pair.second;
      printw("Key: %s\n", key.c_str());
      np.debugPrint(); // Make sure NotePress::debugPrint() is implemented
    }
    printw("===================================\n");
  }

  std::string serialize() { return this->toJson().dump(); };
  KeyboardStream deserialize(std::string &keyboardstream);

  short amplitude = 32767;
  float duration = 0.1f;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(SAMPLERATE *duration));
  std::vector<Effect<float>> effects;

  nlohmann::json toJson() const {
    std::vector<nlohmann::json> synthJson, effectsJson;

    for (int i = 0; i < this->synth.size(); i++) {
      synthJson.push_back(this->synth[i].toJson());
    }
    for (int i = 0; i < this->effects.size(); i++) {
      effectsJson.push_back(this->effects[i].toJson());
    }

    return {{"adsr", this->adsr.toJson()},
            {"effects", effectsJson},
            {"sampleRate", sampleRate},
            {"oscillators", synthJson}};
  }

  int loadJson(const nlohmann::json &j) {
    // ----- availability check -----
    if (!j.contains("sampleRate") || !j.contains("oscillators") ||
        !j.contains("adsr"))
      return -1;

    auto adsrOpt = ADSR::fromJson(j["adsr"]);
    if (!adsrOpt)
      return -1;

    this->sampleRate = j["sampleRate"].get<int>();

    // ----- Build KeyboardStream -----
    this->adsr = *adsrOpt;

    // ----- Effects -----
    this->effects.clear();
    if (j.contains("effects") && j["effects"].is_array()) {
      for (const auto &e : j["effects"]) {
        auto effectOpt = Effect<float>::fromJson(e);
        if (effectOpt)
          this->effects.push_back(*effectOpt);
      }
    }

    // ----- Oscillators ("synth") -----
    this->synth.clear();
    if (j.contains("oscillators") && j["oscillators"].is_array()) {
      for (const auto &o : j["oscillators"]) {
        auto oscOpt = Oscillator::fromJson(o);
        if (oscOpt) {
          this->synth.push_back(*oscOpt);
        }
      }
    }
    return 0;
  }
  /*
    static std::optional<KeyboardStream> fromJson(const nlohmann::json &j) {
      // ----- sample rate -----
      if (!j.contains("sampleRate"))
        return std::nullopt;

      int sr = j["sampleRate"].get<int>();

      // ----- ADSR -----
      if (!j.contains("adsr"))
        return std::nullopt;

      auto adsrOpt = ADSR::fromJson(j["adsr"]);
      if (!adsrOpt)
        return std::nullopt;

      // ----- Build KeyboardStream -----
      KeyboardStream ks(sr);
      ks.adsr = *adsrOpt;

      // ----- Effects -----
      if (j.contains("effects") && j["effects"].is_array()) {
        for (const auto &e : j["effects"]) {
          auto effectOpt = Effect<float>::fromJson(e);
          if (effectOpt)
            ks.effects.push_back(*effectOpt);
        }
      }

      // ----- Oscillators ("synth") -----
      if (j.contains("oscillators") && j["oscillators"].is_array()) {
        for (const auto &o : j["oscillators"]) {
          auto oscOpt = Oscillator::fromJson(o);
          if (oscOpt)
            ks.synth.push_back(*oscOpt);
        }
      }

      return std::optional<KeyboardStream>{std::move(ks)};
    }*/

  void lock() { mtx.lock(); }
  void unlock() { mtx.unlock(); }

private:
  std::map<std::string, std::string> soundMap;
  std::mutex mtx;
  void (*loaderFunc)(unsigned, unsigned) = nullptr;
  std::string soundMapFile;
  int sampleRate;

  std::unordered_map<std::string, Note> notes;
  std::unordered_map<std::string, NotePress> notesPressed;

  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Sound::Rank<float>::Preset rankPreset = Sound::Rank<float>::Preset::None;
  std::unordered_map<std::string, Sound::Rank<float>> ranks;

  float volume = 1.0;

  void setupStandardSynthConfig();

  std::unordered_map<int, std::string> keyPressToNote = {
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
