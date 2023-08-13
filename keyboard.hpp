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

  void prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f);
  void registerNote(std::string &note);
  void registerButtonPress(int note);
  void playNote(std::string &note);

private:
  ALCdevice *device;
  ALCcontext *context;
  std::vector<ALuint> buffers;
  std::vector<ALuint> sources;

  std::map<std::string, int> keyToBufferIndex;
  std::map<std::string, Note> notes;

  std::map<int, std::string> keyPressToNote = {
      {static_cast<int>('a'), "C4"}, {static_cast<int>('s'), "D4"},
      {static_cast<int>('d'), "E4"}, {static_cast<int>('f'), "F4"},
      {static_cast<int>('g'), "G4"}, {static_cast<int>('h'), "A5"},
      {static_cast<int>('j'), "B5"}, {static_cast<int>('k'), "C5"}};
};

#endif
