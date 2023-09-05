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

bool FIR::loadFromFile(std::string file) {
  float maxValue = 1;
  int channels;
  int sampleRate;
  int bps;
  char *data;
  int size;
  if ((data = loadWAV(file.c_str(), channels, sampleRate, bps, size)) != NULL) {
    if (channels == 2) {
      std::vector<short> buffer_right;

      // Only take the left channels IR
      const float maxShortValue =
          static_cast<float>(std::numeric_limits<short>::max());
      splitChannels(data, size, this->buffer, buffer_right);
      (void)buffer_right;
    } else {
      this->buffer = convertToVector(data, size);
    }

    this->impulseResponse.clear();
    this->impulseResponse.reserve(this->buffer.size());
    for (short sample : this->buffer) {
      this->impulseResponse.push_back(sample);
    }
  } else {
    return false;
  }
  return true;
}
