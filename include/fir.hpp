#ifndef KEYBOARD_FIR_HPP
#define KEYBOARD_FIR_HPP

#include "dft.hpp"
#include "waveread.hpp"
#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
class FIR {
public:
  FIR(std::vector<short> &buffer_, int sampleRate_) {
    this->buffer = buffer_;
    this->sampleRate = sampleRate_;
    this->normalize = false;
  }

  FIR(int sampleRate_) { this->sampleRate = sampleRate_; }

  void setBuffer(std::vector<short> buffer_) { this->buffer = buffer_; }
  void setResonance(std::vector<float> alphas, float seconds);
  void setIR(std::vector<float> impulseResponse_) {
    this->impulseResponse = impulseResponse_;
  }
  bool loadFromFile(std::string file);
  short calc(int index);
  void setNormalization(bool normalize) { this->normalize = normalize; };
  bool getNormalization() { return this->normalize; };
  std::vector<short> convolute(int max_size);
  std::vector<float> getIR() { return this->impulseResponse; };

private:
  float ir(int index);
  short buf(int index);
  std::vector<short> buffer;
  std::vector<float> impulseResponse;
  int sampleRate;
  bool normalize;
};

#endif
