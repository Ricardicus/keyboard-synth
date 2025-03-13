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
#include <thread> // for std::this_thread::sleep_for
#include <vector>

#include "effects.hpp"
#include "note.hpp"
#include "notes.hpp"
#include "sound.hpp"

#include "json.hpp"

class Keyboard {
public:
  Keyboard(int maxPolyphony) {
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

    int noteSpace = notes::getNumberOfNotes();
    buffers.resize(noteSpace);
    sources.resize(maxPolyphony);

    // Generate buffer and source
    alGenBuffers(noteSpace, &buffers[0]);
    alGenSources(maxPolyphony, &sources[0]);
  }
  ~Keyboard() {
    // Clean up the buffer and source
    alDeleteSources(this->sources.size(), &this->sources[0]);
    alDeleteBuffers(this->buffers.size(), &this->buffers[0]);
    // Clean up the context and device
    alcMakeContextCurrent(NULL);
    alcDestroyContext(this->context);
    alcCloseDevice(this->device);
  }

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
        if (kv.second.back() == '1') {
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
                    Effects &effects);
  void prepareSound(int sampleRate, ADSR &adsr, Sound::Rank::Preset preset,
                    Effects &effects);
  void registerNote(const std::string &note);
  void registerButtonPress(int note);
  void playNote(const std::string &note);
  void printInstructions();

  void loadSoundMap(std::string soundMapFile) {
    // Open the file for reading
    std::ifstream file(soundMapFile);
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

  std::map<std::string, int> keyToBufferIndex;
  std::map<std::string, Note> notes;

  float volume = 1.0;

  std::map<int, std::string> keyPressToNote = {
      {static_cast<int>('w'), "Db4"}, {static_cast<int>('e'), "Eb4"},
      {static_cast<int>('t'), "Gb4"}, {static_cast<int>('y'), "Ab4"},
      {static_cast<int>('u'), "Bb4"}, {static_cast<int>('y'), "Db5"},
      {static_cast<int>('u'), "Eb5"}, {static_cast<int>('a'), "C4"},
      {static_cast<int>('s'), "D4"},  {static_cast<int>('d'), "E4"},
      {static_cast<int>('f'), "F4"},  {static_cast<int>('g'), "G4"},
      {static_cast<int>('h'), "A4"},  {static_cast<int>('j'), "B4"},
      {static_cast<int>('k'), "C5"},  {static_cast<int>('z'), "C3"},
      {static_cast<int>('x'), "D3"},  {static_cast<int>('c'), "E3"},
      {static_cast<int>('v'), "F3"},  {static_cast<int>('b'), "G3"},
      {static_cast<int>('n'), "A3"},  {static_cast<int>('m'), "B3"},
      {static_cast<int>(','), "C4"},  {static_cast<int>('l'), "D5"}};

  std::map<int, std::function<void()>> keyPressToAction = {
      {static_cast<int>('o'), [this]() { this->changeOctave(-1); }},
      {static_cast<int>('p'), [this]() { this->changeOctave(1); }}};
};
#endif
