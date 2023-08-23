#ifndef KEYBOARD_FIR_HPP
#define KEYBOARD_FIR_HPP

#include <vector>

class FIR {
public:
  FIR(std::vector<short> &buffer_, int sampleRate_) {
    this->buffer = buffer_;
    this->sampleRate = sampleRate_;
  }

  void setResonance(std::vector<float> alphas, float seconds);
  void setIR(std::vector<float> impulseResponse_) {
    this->impulseResponse = impulseResponse_;
  }
  short calc(int index);
  std::vector<short> convolute(int max_size);
  std::vector<float> getIR() { return this->impulseResponse; };

private:
  float ir(int index);
  short buf(int index);
  std::vector<short> buffer;
  std::vector<float> impulseResponse;
  int sampleRate;
};

#endif
