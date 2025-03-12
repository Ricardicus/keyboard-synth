#include <cassert>
#include <chrono> // for std::chrono::seconds
#include <cmath>
#include <iostream>
#include <ncurses.h>
#include <thread> // for std::this_thread::sleep_for
#include <vector>

#include "effects.hpp"
#include "fir.hpp"
#include "keyboard.hpp"
#include "notes.hpp"
#include "waveread.hpp"

void Keyboard::playNote(const std::string &note) {
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

void Keyboard::printInstructions() {
  start_color();

  // Define colors
  init_pair(4, COLOR_WHITE, COLOR_BLACK);  // Labels (White Bold)
  init_pair(5, COLOR_YELLOW, COLOR_BLACK); // Values (Orange/Yellow)

  // Section header
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("These keys are available on your keyboard:\n");
  attroff(COLOR_PAIR(4) | A_BOLD);

  bool first = true;
  for (const auto &kvp : this->keyPressToNote) {
    int press = kvp.first;
    std::string note = kvp.second;

    if (!first)
      printw("\n");
    first = false;

    attron(COLOR_PAIR(5));
    printw("'%c'", static_cast<char>(press));
    attroff(COLOR_PAIR(5));

    attron(COLOR_PAIR(4) | A_BOLD);
    printw(" -> ");
    attroff(COLOR_PAIR(4) | A_BOLD);

    attron(COLOR_PAIR(5));
    printw("%s", note.c_str());
    attroff(COLOR_PAIR(5));

    auto it = this->notes.find(note);
    if (it != this->notes.end()) {
      Note &n = it->second;

      attron(COLOR_PAIR(4) | A_BOLD);
      printw(" (freq: ");
      attroff(COLOR_PAIR(4) | A_BOLD);

      attron(COLOR_PAIR(5));
      printw("%.2f Hz", n.frequency);
      attroff(COLOR_PAIR(5));

      attron(COLOR_PAIR(4) | A_BOLD);
      printw(", length: ");
      attroff(COLOR_PAIR(4) | A_BOLD);

      attron(COLOR_PAIR(5));
      printw("%.2f s", (float)n.length / (float)n.sampleRate);
      attroff(COLOR_PAIR(5));

      attron(COLOR_PAIR(4) | A_BOLD);
      printw(")");
      attroff(COLOR_PAIR(4) | A_BOLD);
    }
  }

  printw("\n");

  // Volume knob
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("\nVolume knob set to: ");
  attroff(COLOR_PAIR(4) | A_BOLD);

  attron(COLOR_PAIR(5));
  printw("%.2f\n", this->volume);
  attroff(COLOR_PAIR(5));

  attron(COLOR_PAIR(4) | A_BOLD);
  printw("Press 'o'/'p' to +/- one octave\n");
  attroff(COLOR_PAIR(4) | A_BOLD);
}

void Keyboard::prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f,
                            Effects &effects) {
  int length = adsr.getLength();
  std::vector<std::string> notes = notes::getNotes();
  assert(notes.size() == this->buffers.size());
  int bufferIndex = 0;
  unsigned ticks = notes.size();
  unsigned tick = 1;

  for (const auto &key : notes) {

    Note n = Note(key, length, sampleRate);
    if (f == Sound::WaveForm::WaveFile &&
        this->soundMap.find(key) != this->soundMap.end()) {
      int channels;
      int sampleRate;
      int bps;
      char *data;
      int size;
      ALenum format = AL_FORMAT_MONO8;
      if ((data = loadWAV(this->soundMap[key], channels, sampleRate, bps,
                          size)) != NULL) {
        if (channels == 1) {
          if (bps == 8) {
            format = AL_FORMAT_MONO8;
          } else {
            format = AL_FORMAT_MONO16;
          }
        } else {
          if (bps == 8) {
            format = AL_FORMAT_STEREO8;
          } else {
            format = AL_FORMAT_STEREO16;
          }
        }

        if (channels == 2) {
          std::vector<short> buffer_left;
          std::vector<short> buffer_right;

          splitChannels(data, size, buffer_left, buffer_right);

          std::vector<short> buffer_left_effect = effects.apply(buffer_left);
          std::vector<short> buffer_right_effect = effects.apply(buffer_right);

          std::vector<short> interleaved;
          interleaved.reserve(buffer_left_effect.size() *
                              2); // Reserve space to optimize performance

          for (size_t i = 0; i < buffer_left_effect.size(); ++i) {
            interleaved.push_back(buffer_left_effect[i] * this->volume);
            interleaved.push_back(buffer_right_effect[i] * this->volume);
          }

          alBufferData(this->buffers[bufferIndex], format, interleaved.data(),
                       size, sampleRate);

        } else {

          std::vector<short> buffer_mono = convertToVector(data, size);
          std::vector<short> buffer = effects.apply(buffer_mono);
          for (size_t i = 0; i < buffer.size(); ++i) {
            buffer[i] = buffer[i] * this->volume;
          }

          alBufferData(this->buffers[bufferIndex], format, buffer.data(), size,
                       sampleRate);
        }

      } else {
        fprintf(stderr, "Error loading file: %s\n",
                this->soundMap[key].c_str());
      }

    } else {
      std::vector<short> buffer_raw = Sound::generateWave(f, n, adsr);
      std::vector<short> buffer = effects.apply(buffer_raw);
      for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = buffer[i] * this->volume;
      }
      n.setBuffer(buffer);

      alBufferData(this->buffers[bufferIndex], AL_FORMAT_MONO16,
                   n.buffer.data(), n.buffer.size() * sizeof(short),
                   n.sampleRate);
    }
    if (this->loaderFunc) {
      this->loaderFunc(ticks, tick);
      tick++;
    }

    this->keyToBufferIndex[key] = bufferIndex;
    this->notes.insert(std::make_pair(key, n));
    bufferIndex++;
  }
}

void Keyboard::registerNote(const std::string &note) { playNote(note); }

void Keyboard::registerButtonPress(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    playNote(note);
  }
  if (this->keyPressToAction.find(pressed) != this->keyPressToAction.end()) {
    this->keyPressToAction[pressed]();
  }
}
