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
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <thread> // for std::this_thread::sleep_for
#include <vector>

#include "note.hpp"
#include "notes.hpp"
#include "sound.hpp"

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

  void prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f);
  void registerNote(std::string &note);
  void registerButtonPress(int note);
  void playNote(std::string &note);
  void printInstructions();

private:
  ALCdevice *device;
  ALCcontext *context;
  std::vector<ALuint> buffers;
  std::vector<ALuint> sources;

  std::map<std::string, int> keyToBufferIndex;
  std::map<std::string, Note> notes;

  std::map<int, std::string> keyPressToNote = {
      {static_cast<int>('w'), "C#4"}, {static_cast<int>('e'), "D#4"},
      {static_cast<int>('t'), "F#4"}, {static_cast<int>('y'), "G#4"},
      {static_cast<int>('u'), "A#4"}, {static_cast<int>('y'), "C#5"},
      {static_cast<int>('u'), "D#5"}, {static_cast<int>('a'), "C4"},
      {static_cast<int>('s'), "D4"},  {static_cast<int>('d'), "E4"},
      {static_cast<int>('f'), "F4"},  {static_cast<int>('g'), "G4"},
      {static_cast<int>('h'), "A4"},  {static_cast<int>('j'), "B4"},
      {static_cast<int>('k'), "C5"},  {static_cast<int>('z'), "C3"},
      {static_cast<int>('x'), "D3"},  {static_cast<int>('c'), "E3"},
      {static_cast<int>('v'), "F3"},  {static_cast<int>('b'), "G3"},
      {static_cast<int>('n'), "A3"},  {static_cast<int>('m'), "B3"},
      {static_cast<int>(','), "C4"}};

  std::map<int, std::function<void()>> keyPressToAction = {
      {static_cast<int>('o'), [this]() { this->changeOctave(-1); }},
      {static_cast<int>('p'), [this]() { this->changeOctave(1); }}};
};
#endif
