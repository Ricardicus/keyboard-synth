#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
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

void KeyboardStream::prepareSound(int sampleRate, ADSR &adsr,
                                  std::vector<Effect> &effects) {
  this->adsr = adsr;
  this->sampleRate = sampleRate;
  this->effects = effects;
  this->setupStandardSynthConfig();
}

void KeyboardStream::setupStandardSynthConfig() {
  this->synth.reserve(4);
  for (int i = 0; i < 4; i++) {
    this->synth.emplace_back(SAMPLERATE);
  }
  for (int i = 0; i < 4; i++) {
    this->synth[i].setVolume(i == 0 ? 0.5 : 0);
  }
}

void KeyboardStream::registerNote(const std::string &note) {
  auto it = this->notesPressed.find(note);

  if (it != this->notesPressed.end()) {
    // Note already exists – create a new entry with release = true
    NotePress releasedNote;
    releasedNote = it->second;
    releasedNote.release = true;
    releasedNote.phase = 0;
    releasedNote.time = KeyboardStream::currentTimeMillis();
    std::string newKey = note + "--" + std::to_string(releasedNote.time);
    this->notesPressed[newKey] = releasedNote;
  }

  // Always create a fresh note entry
  NotePress np;
  np.time = KeyboardStream::currentTimeMillis();
  np.note = note;
  np.adsr = this->adsr;
  np.frequency = notes::getFrequency(note);
  np.index = 0;
  np.release = false;

  this->notesPressed[note] = np;
}

void KeyboardStream::registerNoteRelease(const std::string &note) {
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

void KeyboardStream::fillBuffer(float *buffer, const int len) {
  float deltaT = 1.0f / this->sampleRate;

  auto start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < len; i++) {
    float sample = 0.0f;

    std::lock_guard<std::mutex> guard(this->mtx);
    for (auto it = notesPressed.begin(); it != notesPressed.end();) {
      NotePress &note = it->second;
      int index = note.index;
      float adsr;
      double freq = note.frequency;

      // Use ADSR envelope
      if (note.adsr.reached_sustain(index) && !note.release) {
        adsr = static_cast<float>(note.adsr.sustain());
      } else {
        adsr = static_cast<float>(note.adsr.response(index));
        note.index++;
      }

      sample += adsr * generateSample(note.note, note.phase, note.rankIndex);
      note.rankIndex++;

      // Advance phase
      note.phase += 2.0f * M_PI * freq * deltaT;
      if (note.phase > 2.0f * M_PI)
        note.phase -= 2.0f * M_PI;

      // Remove if done
      if (note.index >= note.adsr.getLength()) {
        it = notesPressed.erase(it);
      } else {
        ++it;
      }
    }

    // Output sample
    float entry = sample * this->gain;

    entry = this->echo.process(entry);

    buffer[i] = entry;
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;

  // Optionally keep this if debugging:
  // printw(" %f (%zu) -", duration.count(), notesPressed.size());
  refresh();
}

float KeyboardStream::generateSample(std::string note, float phase, int index) {
  float result = 0;
  float min = static_cast<float>(std::numeric_limits<short>::min());
  float max = static_cast<float>(std::numeric_limits<short>::max());

  for (Oscillator &oscillator : this->synth) {
    result += oscillator.volume * oscillator.getSample(note, index);
  }

  result =
      std::clamp(result, static_cast<float>(std::numeric_limits<short>::min()),
                 static_cast<float>(std::numeric_limits<short>::max()));

  return result;
}

void KeyboardStream::Oscillator::setVolume(float volume) {
  this->volume = volume;
}

using Pipe = std::pair<Note, Sound::WaveForm>;
void KeyboardStream::Oscillator::setOctave(int octave) {
  this->octave = octave;
  this->updateFrequencies();
}

void KeyboardStream::Oscillator::setDetune(int detune) {
  this->detune = detune;
  this->updateFrequencies();
}

void KeyboardStream::Oscillator::setADSR(ADSR adsr) { this->adsr = adsr; }

void KeyboardStream::Oscillator::setSound(Sound::Rank::Preset sound) {
  this->sound = sound;
}

float KeyboardStream::Oscillator::getSample(const std::string &note,
                                            int index) {
  if (!this->initialized) {
    this->initialize();
  }
  float result = 0;
  if (this->ranks.find(note) != this->ranks.end()) {
    result = this->ranks[note].generateRankSampleIndex(index);
  }
  return result;
}

void KeyboardStream::Oscillator::reset(const std::string &note) {
  if (this->ranks.find(note) != this->ranks.end()) {
    this->ranks[note].reset();
  }
}

void KeyboardStream::Oscillator::initialize() {
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
      Note n = Note(key, this->adsr.length, this->sampleRate);

      float frequency = notes::getFrequency(key);
      Sound::Rank r = Sound::Rank::fromPreset(this->sound, frequency,
                                              this->adsr.length, sampleRate);
      { this->ranks[key] = r; }
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

std::string KeyboardStream::Oscillator::printSynthConfig() const {
  std::ostringstream out;

  out << "Synth Configuration:\n";
  out << "--------------------\n";
  out << "Sample Rate: " << sampleRate << "\n";
  out << "Volume: " << volume << "\n";
  out << "Octave: " << octave << "\n";
  out << "Detune: " << detune << "\n";
  out << "Sound Preset: " << Sound::Rank::presetStr(sound) << "\n";

  out << "ADSR Envelope:\n";
  out << "  Amplitude: " << adsr.amplitude << "\n";
  out << "  Quantas: " << adsr.quantas << "\n";
  out << "  QADSR: [" << adsr.qadsr[0] << ", " << adsr.qadsr[1] << ", "
      << adsr.qadsr[2] << ", " << adsr.qadsr[3] << "]\n";
  out << "  Length: " << adsr.length << "\n";
  out << "  Quantas Length: " << adsr.quantas_length << "\n";
  out << "  Sustain Level: " << adsr.sustain_level << "\n";

  return out.str();
}

void KeyboardStream::printSynthConfig() const {
  int synthIndex = 1;
  for (const Oscillator &oscillator : this->synth) {
    // Section header
    attron(COLOR_PAIR(7) | A_BOLD);
    printw("======== Oscillator %d ========\n", synthIndex);
    attroff(COLOR_PAIR(7) | A_BOLD);

    attron(COLOR_PAIR(4));
    printw("%s", oscillator.printSynthConfig().c_str());
    attroff(COLOR_PAIR(4));
  }
}
