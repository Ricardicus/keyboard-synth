#include <cassert>
#include <chrono> // for std::chrono::seconds
#include <cmath>
#include <iostream>
#include <ncurses.h>
#include <thread> // for std::this_thread::sleep_for
#include <vector>

#include "effect.hpp"
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
  init_pair(6, COLOR_BLACK, COLOR_WHITE);  // Key characters
  init_pair(7, COLOR_BLUE, COLOR_WHITE);   // Note names

  // Section header
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("These keys are available on your keyboard:\n");
  attroff(COLOR_PAIR(4) | A_BOLD);

  std::vector<int> rowNumber = {'1', '2', '3', '4', '5',
                                '6', '7', '8', '9', '0'};
  std::vector<int> rowQwert = {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'};
  std::vector<int> rowHome = {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l'};
  std::vector<int> rowBottom = {'z', 'x', 'c', 'v', 'b', 'n', 'm', ','};

  // Helper lambda to print a row.
  auto printRow = [this](const std::vector<int> &row, const char *prefix) {
    attron(COLOR_PAIR(4) | A_BOLD);
    printw("%s| ", prefix);
    attroff(COLOR_PAIR(4) | A_BOLD);
    for (int key : row) {
      auto it = this->keyPressToNote.find(key);
      if (it != this->keyPressToNote.end()) {
        attron(COLOR_PAIR(6));
        printw("%c ", key);
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(7));
        printw("[%s]", it->second.c_str());
        attroff(COLOR_PAIR(7));
        attron(COLOR_PAIR(4) | A_BOLD);
        printw(" | ");
        attroff(COLOR_PAIR(4) | A_BOLD);
      }
    }
    printw("\n");
  };

  // Print each row in order.
  printRow(rowNumber, "");
  printRow(rowQwert, "  ");
  printRow(rowHome, "    ");
  printRow(rowBottom, "      ");

  // Volume knob display
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("\nVolume knob set to: ");
  attroff(COLOR_PAIR(4) | A_BOLD);
  attron(COLOR_PAIR(5));
  printw("%.2f\n", this->volume);
  attroff(COLOR_PAIR(5));

  attron(COLOR_PAIR(4) | A_BOLD);
  printw("Press 'p'/'o' to +/- one octave\n");
  attroff(COLOR_PAIR(4) | A_BOLD);
}

void Keyboard::prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f,
                            std::vector<Effect> &effects) {
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
          std::vector<short> buffer_left_in;
          std::vector<short> buffer_right_in;

          splitChannels(data, size, buffer_left_in, buffer_right_in);

          std::vector<short> buffer_left_effect_out;
          for (int i = 0; i < effects.size(); i++) {
            buffer_left_effect_out = effects[i].apply(buffer_left_in);
            if (effects.size() != i + 1) {
              buffer_left_in = buffer_left_effect_out;
            }
          }

          std::vector<short> buffer_right_effect_out;
          for (int i = 0; i < effects.size(); i++) {
            buffer_right_effect_out = effects[i].apply(buffer_right_in);
            if (effects.size() != i + 1) {
              buffer_right_in = buffer_right_effect_out;
            }
          }

          std::vector<short> interleaved;
          interleaved.reserve(buffer_left_effect_out.size() *
                              2); // Reserve space to optimize performance

          for (size_t i = 0; i < buffer_left_effect_out.size(); ++i) {
            interleaved.push_back(buffer_left_effect_out[i] * this->volume);
            interleaved.push_back(buffer_right_effect_out[i] * this->volume);
          }

          alBufferData(this->buffers[bufferIndex], format, interleaved.data(),
                       size, sampleRate);

        } else {
          std::vector<short> buffer_in = convertToVector(data, size);
          std::vector<short> buffer_out;

          for (int i = 0; i < effects.size(); i++) {
            buffer_out = effects[i].apply(buffer_in);
            if (effects.size() != i + 1) {
              buffer_in = buffer_out;
            }
          }

          alBufferData(this->buffers[bufferIndex], format, buffer_out.data(),
                       size, sampleRate);
        }

      } else {
        fprintf(stderr, "Error loading file: %s\n",
                this->soundMap[key].c_str());
      }

    } else {

      // No wave file for this key, fill in the blanks with sine
      if (this->soundMapFile.size() > 0) {
        printf("warning: Missing key '%s' in the mapping file '%s', filling in "
               "with a sine wave\n",
               key.c_str(), this->soundMapFile.c_str());
      }
      std::vector<short> buffer_in =
          Sound::generateWave(Sound::WaveForm::Sine, n, adsr);
      std::vector<short> buffer_out;

      for (int i = 0; i < effects.size(); i++) {
        buffer_out = effects[i].apply(buffer_in);
        if (effects.size() != i + 1) {
          buffer_in = buffer_out;
        }
      }
      for (size_t i = 0; i < buffer_out.size(); ++i) {
        buffer_out[i] = buffer_out[i] * this->volume;
      }
      n.setBuffer(buffer_out);

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

void Keyboard::prepareSound(int sampleRate, ADSR &adsr,
                            Sound::Rank::Preset preset,
                            std::vector<Effect> &effects) {
  std::vector<std::string> notes = notes::getNotes();
  assert(notes.size() == this->buffers.size());
  int bufferIndex = 0;
  unsigned ticks = notes.size();
  unsigned tick = 1;

  for (const auto &key : notes) {
    // Bottom row (one octave lower than home row)
    Note n = Note(key, adsr.length, sampleRate);
    Sound::Rank r;
    switch (preset) {
    case Sound::Rank::Preset::SuperSaw:
      r = Sound::Rank::superSaw(n.frequency, adsr.length, sampleRate);
    case Sound::Rank::Preset::None:
      break;
    }
    r.adsr = adsr;

    std::vector<short> buffer_in = Sound::generateWave(r);
    std::vector<short> buffer_out;

    for (int i = 0; i < effects.size(); i++) {
      buffer_out = effects[i].apply(buffer_in);
      if (effects.size() != i + 1) {
        buffer_in = buffer_out;
      }
    }
    for (size_t i = 0; i < buffer_out.size(); ++i) {
      buffer_out[i] = buffer_out[i] * this->volume;
    }
    n.setBuffer(buffer_out);

    alBufferData(this->buffers[bufferIndex], AL_FORMAT_MONO16, n.buffer.data(),
                 n.buffer.size() * sizeof(short), n.sampleRate);

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
