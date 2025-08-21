#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP

#ifdef USE_OPENAL_PREFIX
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#elif USE_AL_PREFIX
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>  // Guessing. Use the definitions above for compilation.
#include <OpenAL/alc.h> // Guessing. Use the definitions above for compilation.
#endif

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

class Keyboard {
public:
  Keyboard(int maxPolyphony) { setup(maxPolyphony); }
  ~Keyboard() { teardown(); }

  void setup(int maxPolyphony) {
    // Initialize the device and context
    device = alcOpenDevice(NULL);
    if (!device) {
      // Handle error
      std::cerr << "Keyboard, error: Failed to create device driver"
                << std::endl;
      exit(1);
    }
    context = alcCreateContext(device, NULL);
    if (!context) {
      // Handle error
      std::cerr << "Keyboard, error: Failed to create device driver context"
                << std::endl;
      exit(1);
    }
    alcMakeContextCurrent(context);

    int noteSpace = notes::getNumberOfNotes(this->tuning);
    buffers.resize(noteSpace);
    sources.resize(maxPolyphony);

    // Generate buffer and source
    alGenBuffers(noteSpace, &buffers[0]);
    alGenSources(maxPolyphony, &sources[0]);
  }

  void teardown() {
    // Clean up the buffer and source
    alDeleteSources(this->sources.size(), &this->sources[0]);
    alDeleteBuffers(this->buffers.size(), &this->buffers[0]);
    // Clean up the context and device
    alcMakeContextCurrent(NULL);
    alcDestroyContext(this->context);
    alcCloseDevice(this->device);
  }

  float getVolume() { return this->volume; }
  void setVolume(float volume_) {
    this->volume = volume_;
    if (this->volume < 0.0)
      this->volume = 0;
    if (this->volume > 1.0)
      this->volume = 1.0;
  }

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
                    std::vector<Effect<short>> &effects, int nbrThreads);
  void prepareSound(int sampleRate, ADSR &adsr,
                    Sound::Rank<short>::Preset preset,
                    std::vector<Effect<short>> &effects, int nbrThreads);
  void registerNote(const std::string &note);
  void registerButtonPress(int note);
  void playNote(const std::string &note);
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

private:
  ALCdevice *device;
  ALCcontext *context;
  std::vector<ALuint> buffers;
  std::vector<ALuint> sources;
  std::map<std::string, std::string> soundMap;
  void (*loaderFunc)(unsigned, unsigned) = nullptr;
  std::string soundMapFile;
  notes::TuningSystem tuning = notes::TuningSystem::EqualTemperament;

  std::map<std::string, int> keyToBufferIndex;
  std::map<std::string, Note> notes;

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
  std::mutex mtx;
};
#endif
