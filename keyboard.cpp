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

void Keyboard::printInstructions() {
  bool first = true;
  printw("These keys are available on your keyboard:\n");
  for (const auto &kvp : this->keyPressToNote) {
    int press = kvp.first;
    std::string note = kvp.second;
    std::string note_info = "";
    auto it = this->notes.find(note);

    if (it != this->notes.end()) {
      Note &n = it->second;
      note_info = ", freq: " + std::to_string(n.frequency) + " hz, length: " +
                  std::to_string((float)n.length / (float)n.sampleRate) + " s)";
    }

    printw("%s'%c' -> %s%s", (first ? "" : "\n"), static_cast<char>(press),
           note.c_str(), note_info.c_str());
    first = false;
  }
  printw("\n\n");
}

std::vector<short> convertToVector(const char *data, int numSamples) {
  // Cast the char pointer to a short pointer.
  const short *shortData = reinterpret_cast<const short *>(data);
  return std::vector<short>(shortData, shortData + numSamples);
}

void splitChannels(const char *data, size_t dataSize, std::vector<short> &left,
                   std::vector<short> &right) {
  // Ensure the data is a multiple of 4 bytes (2 channels * 2 bytes per sample)
  if (dataSize % 4 != 0) {
    std::cerr << "Invalid data size!" << std::endl;
    return;
  }

  size_t numStereoPairs = dataSize / 4; // Total number of left-right pairs
  size_t numSamplesPerChannel =
      dataSize / 2 / 2; // Number of samples for each channel

  left.resize(numSamplesPerChannel);
  right.resize(numSamplesPerChannel);

  for (size_t i = 0, j = 0; i < dataSize; i += 4, j++) {

    left[j] =
        static_cast<short>((static_cast<unsigned char>(data[i + 1]) << 8) |
                           static_cast<unsigned char>(data[i]));
    right[j] =
        static_cast<short>((static_cast<unsigned char>(data[i + 3]) << 8) |
                           static_cast<unsigned char>(data[i + 2]));
  }
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
            interleaved.push_back(buffer_left_effect[i]);
            interleaved.push_back(buffer_right_effect[i]);
          }

          alBufferData(this->buffers[bufferIndex], format, interleaved.data(),
                       size, sampleRate);

        } else {

          std::vector<short> buffer_mono = convertToVector(data, size);
          std::vector<short> buffer = effects.apply(buffer_mono);

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

void Keyboard::registerNote(std::string &note) { playNote(note); }

void Keyboard::registerButtonPress(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    playNote(note);
  }
  if (this->keyPressToAction.find(pressed) != this->keyPressToAction.end()) {
    this->keyPressToAction[pressed]();
  }
}
