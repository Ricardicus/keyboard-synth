#include "fir.hpp"

std::vector<short> FIR::convolute(int max_size) {
  std::vector<short> result;
  int i = 0;
  while (i < max_size) {
    short res = this->calc(i);
    result.push_back(res);
    ++i;
  }
  return result;
}

std::vector<short> FIR::getIR() {
  std::vector<short> result;
  int i = 0;
  while (i < this->impulseResponse.size()) {
    result.push_back(static_cast<short>(32767 * this->impulseResponse[i]));
    ++i;
  }
  return result;
}

short FIR::calc(int index) {
  short sum = 0;
  int i = 0;
  while (i < index) {
    sum += static_cast<short>(buf(index - i) * ir(i));
    ++i;
  }
  return sum;
}

void FIR::setResonance(std::vector<float> alphas, float seconds) {
  int interval = static_cast<int>(this->sampleRate * seconds);
  this->impulseResponse.clear();
  int size = alphas.size() * interval;
  int i = 0;
  int alpha_index = 0;
  while (i < size) {
    if (i % interval == 0) {
      this->impulseResponse.push_back((-1 ? i % 1 == 0 : 1) *
                                      alphas[alpha_index]);
      ++alpha_index;
    } else {
      this->impulseResponse.push_back(0);
    }
    ++i;
  }
}

float FIR::ir(int index) {
  if (index > this->impulseResponse.size() || index < 0) {
    return 0;
  }
  return this->impulseResponse[index];
}

short FIR::buf(int index) {
  if (index < 0 || index >= this->buffer.size()) {
    return 0;
  }
  return this->buffer[index];
}
