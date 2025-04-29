#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <ncurses.h>
#include <thread>
#include <vector>

#include "effect.hpp"
#include "fir.hpp"
#include "keyboardstream.hpp"
#include "notes.hpp"
#include "waveread.hpp"

void KeyboardStream::printInstructions() {
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
  printw("Press 'P'/'O' to +/- keyboard preset sounds\n");
  attroff(COLOR_PAIR(4) | A_BOLD);
}

void KeyboardStream::prepareSound(int sampleRate, ADSR &adsr, Sound::WaveForm f,
                                  std::vector<Effect> &effects,
                                  int nbrThreads) {
  this->adsr = adsr;
  this->waveForm = f;
}

void KeyboardStream::prepareSound(int sampleRate, ADSR &adsr,
                                  Sound::Rank::Preset preset,
                                  std::vector<Effect> &effects,
                                  int nbrThreads) {
  this->adsr = adsr;
  this->rankPreset = preset;
  for (const std::string &note : notes::getNotes()) {
    float frequency = static_cast<float>(notes::getFrequency(note));
    this->ranks[note] =
        Sound::Rank::fromPreset(preset, frequency, adsr.length, sampleRate);
    ;
  }
}

void KeyboardStream::registerNote(const std::string &note) {
  auto it = this->notesPressed.find(note);
  if (it != this->notesPressed.end()) {
    // Note already exists, just update the time
    this->ranks[it->second.note].reset();
    it->second.index = 0;
    it->second.release = false;
    it->second.time = KeyboardStream::currentTimeMillis();
  } else {
    // Note doesn't exist, create a new NotePress
    NotePress np;
    np.time = KeyboardStream::currentTimeMillis();
    np.note = note;
    np.adsr = this->adsr;
    np.index = 0;
    this->notesPressed[note] = np;
  }
}

void KeyboardStream::registerNoteRelease(const std::string &note) {
  std::lock_guard<std::mutex> lock(this->mtx);
  auto it = this->notesPressed.find(note);
  if (it != this->notesPressed.end()) {
    it->second.release = true;
  }
}

void KeyboardStream::registerButtonPress(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    registerNote(note);
  }
  if (this->keyPressToAction.find(pressed) != this->keyPressToAction.end()) {
    this->keyPressToAction[pressed]();
  }
}

void KeyboardStream::registerButtonRelease(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    registerNoteRelease(note);
  }
}

void KeyboardStream::fillBuffer(short *buffer, const int len) {
  float deltaT = 1.0f / this->sampleRate;

  for (int i = 0; i < len; i++) {
    int sample = 0.0;
    short factor = this->notesPressed.size();

    std::lock_guard<std::mutex> lock(this->mtx);

    for (auto it = notesPressed.begin(); it != notesPressed.end();) {
      NotePress &note = it->second;
      int index = note.index;
      int adsr = note.adsr.response(index);
      double freq = notes::getFrequency(note.note);

      // Add sine wave at current phase
      if (note.adsr.reached_sustain(index) && !note.release) {
        adsr = note.adsr.sustain();
      } else {
        // Advance index for the next sample
        adsr = note.adsr.response(index);
        note.index++;
      }

      sample += static_cast<short>(static_cast<float>(adsr) *
                                   generateSample(note.note, note.phase));
      // Advance phase for next sample
      note.phase += 2.0f * M_PI * freq * deltaT;
      if (note.phase > 2.0f * M_PI)
        note.phase -= 2.0f * M_PI; // Wrap around for stability

      if (note.index >= note.adsr.getLength()) {
        it = notesPressed.erase(it);
      } else {
        ++it;
      }
    }

    // Normalize and write to buffer
    buffer[i] = static_cast<short>(sample / 10); // Adjust volume
  }
}

float KeyboardStream::generateSample(std::string note, float phase) {
  if (this->rankPreset != Sound::Rank::Preset::None) {
    switch (this->rankPreset) {
    case Sound::Rank::Preset::Sine:
      return Sound::sinus(phase);

    case Sound::Rank::Preset::Triangular:
      return Sound::triangular(phase);

    case Sound::Rank::Preset::Square:
      return Sound::square(phase);

    case Sound::Rank::Preset::Saw:
      return Sound::saw(phase);

    default: {
      // Custom synthesis logic needed
      return this->ranks[note].generateRankSample();
    }
    }
  }

  // Fallback to basic waveform selection
  switch (this->waveForm) {
  case Sound::Sine:
    return Sound::sinus(phase);

  case Sound::Triangular:
    return Sound::triangular(phase);

  case Sound::Square:
    return Sound::square(phase);

  case Sound::Saw:
    return Sound::saw(phase);

  case Sound::WaveFile:
    // Placeholder for WaveFile sample playback
    return 0.0f;

  default:
    return 0.0f;
  }
}

void KeyboardStream::Synth::setVolume(float volume) { this->volume = volume; }

using Pipe = std::pair<Note, Sound::WaveForm>;
void KeyboardStream::Synth::setOctave(int octave) {
  this->octave = octave;
  this->updateFrequencies();
}

void KeyboardStream::Synth::setDetune(int detune) {
  this->detune = detune;
  this->updateFrequencies();
}

void KeyboardStream::Synth::setADSR(ADSR adsr) { this->adsr = adsr; }

void KeyboardStream::Synth::setSound(Sound::Rank::Preset sound) {
  this->sound = sound;
}

void KeyboardStream::Synth::initialize(int sampleRate) {
  std::vector<std::string> notes = notes::getNotes();
  // Ensure nbrThreads is sensible
  int nbrThreads = 4;
  nbrThreads =
      std::max(1, std::min(nbrThreads, static_cast<int>(notes.size())));

  // Shared data needs protection
  std::mutex mtx;

  // Split notes into chunks
  std::vector<std::thread> threads;
  int notesPerThread = notes.size() / nbrThreads;
  int remainder = notes.size() % nbrThreads;

  // Lambda to process a range of notes
  auto processNotes = [&](int start, int end, int threadIndex) {
    for (int bufferIndex = start; bufferIndex < end; ++bufferIndex) {
      const auto &key = notes[bufferIndex];
      // Bottom row (one octave lower than home row)
      Note n = Note(key, this->adsr.length, sampleRate);

      float frequency = notes::getFrequency(key);
      Sound::Rank r = Sound::Rank::fromPreset(this->sound, frequency,
                                              this->adsr.length, sampleRate);
      {
        std::lock_guard<std::mutex> lock(mtx);
        this->ranks[key] = r;
      }
    }
  };
  // Launch threads
  int start = 0;
  for (int t = 0; t < nbrThreads; ++t) {
    int extra = (t < remainder) ? 1 : 0; // Distribute remainder
    int end = start + notesPerThread + extra;
    threads.emplace_back(processNotes, start, end, t);
    start = end;
  }

  // Wait for all threads to finish
  for (auto &thread : threads) {
    thread.join();
  }

  this->initialized = true;
}
