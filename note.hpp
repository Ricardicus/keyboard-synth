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
  void setBuffer(std::vector<short> &buffer) { this->buffer = buffer; }
  std::vector<short> buffer;
  float frequency;
  int sampleRate;
  int length;
};

#endif
