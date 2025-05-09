#ifndef KEYBOARD_NOTE_HPP
#define KEYBOARD_NOTE_HPP

#include "notes.hpp"
#include <vector>

class Note {
public:
  Note(std::string note, int length, int sampleRate) {
    this->frequency = notes::getFrequency(note);
    this->length = length;
    this->sampleRate = sampleRate;
  }
  Note(float frequency, int length, int sampleRate) {
    this->frequency = frequency;
    this->length = length;
    this->sampleRate = sampleRate;
  }
  Note() {}
  void setBuffer(std::vector<short> &buffer) { this->buffer = buffer; }
  std::vector<short> buffer;
  float frequency = 0;
  float frequencyAltered = 0;
  int sampleRate = 0;
  int length = 0;
  float volume = 1.0;
};

#endif
