#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <chrono> // for std::chrono::seconds
#include <cmath>
#include <iostream>
#include <thread> // for std::this_thread::sleep_for
#include <vector>

#include <ncurses.h>

#include "keyboard.hpp"
#include "notes.hpp"

void Keyboard::playNote(std::string &note) {
  if (this->keyToBufferIndex.find(note) == this->keyToBufferIndex.end()) {
    // Failed to find the note
    return;
  }

  int noteIndex = this->keyToBufferIndex[note];
  if (noteIndex < 0 || noteIndex >= buffers.size())
    return;

  // Find an available source
  ALuint source = 0;
  for (auto &s : this->sources) {
    ALint state;
    alGetSourcei(s, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
      source = s;
      break;
    }
  }
  if (source) {
    alSourcei(source, AL_BUFFER, buffers[noteIndex]);
    alSourcePlay(source);
  } else {
    // All sources are busy; consider increasing maxPolyphony or handle this
    // case accordingly
  }
}

void Keyboard::prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f) {
  int length = adsr.getLength();
  std::vector<std::string> notes = notes::getNotes();
  assert(notes.size() == this->buffers.size());
  int bufferIndex = 0;
  for (const auto &key : notes) {
    Note n = Note(key, length, sampleRate);
    std::vector<short> buffer = Sound::generateWave(f, n, adsr);
    n.setBuffer(buffer);
    alBufferData(this->buffers[bufferIndex], AL_FORMAT_MONO16, n.buffer.data(),
                 n.buffer.size() * sizeof(short), n.sampleRate);
    this->keyToBufferIndex[key] = bufferIndex;
    this->notes.insert(std::make_pair(key, n));
    bufferIndex++;
  }
}

void Keyboard::registerNote(std::string &note) {
  playNote(note);
}

void Keyboard::registerButtonPress(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    playNote(note);
  }
}
