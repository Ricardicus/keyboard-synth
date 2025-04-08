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
                                  int nbrThreads) {}

void KeyboardStream::prepareSound(int sampleRate, ADSR &adsr,
                                  Sound::Rank::Preset preset,
                                  std::vector<Effect> &effects,
                                  int nbrThreads) {}

void KeyboardStream::registerNote(const std::string &note) {
  NotePress np;

  np.time = KeyboardStream::currentTimeMillis();
  np.note = note;
  np.isPlaying = true;
  {
    std::lock_guard<std::mutex> lock(this->mtx);
    this->notesPressed[note] = np;
  }
}

void KeyboardStream::registerNoteRelease(const std::string &note) {
  std::lock_guard<std::mutex> lock(this->mtx);
  auto it = this->notesPressed.find(note);
  if (it != this->notesPressed.end()) {
    this->notesPressed.erase(it);
  }
}

void KeyboardStream::registerButtonPress(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    registerNote(note);
  } else {
    printf("Note not found %d\n", pressed);
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

  std::lock_guard<std::mutex> lock(this->mtx);

  for (int i = 0; i < len; i++) {
    float sample = 0.0f;

    for (auto &pair : this->notesPressed) {
      NotePress &note = pair.second;
      double freq = notes::getFrequency(note.note);

      // Add sine wave at current phase
      sample += std::sin(note.phase);

      // Advance phase for next sample
      note.phase += 2.0f * M_PI * freq * deltaT;
      if (note.phase > 2.0f * M_PI)
        note.phase -= 2.0f * M_PI; // Wrap around for stability
    }

    // Normalize and write to buffer
    buffer[i] = sample * 0.2f; // Adjust volume
  }
}
