#include <complex>
#include <vector>

#include "dft.hpp"
#include "effects.hpp"

using Complex = std::complex<double>;

std::vector<short> Effects::apply(std::vector<short> &buffer) {
  FourierTransform ft;
  std::vector<short> output;
  std::vector<short> buffer_ = buffer;
  for (int i = 0; i < this->firs.size(); i++) {
    auto impulse = this->firs[i].getIR();
    size_t paddedSize = buffer_.size() + impulse.size() - 1;
    size_t nearestPowerOfTwo = std::pow(2, std::ceil(std::log2(paddedSize)));

    // Pad waveData and impulse response to nearestPowerOfTwo
    while (buffer_.size() < nearestPowerOfTwo) {
      buffer_.push_back(0);
    }

    while (impulse.size() < nearestPowerOfTwo) {
      impulse.push_back(0);
    }

    std::vector<Complex> dft_ir = ft.DFT(impulse);
    std::vector<Complex> dft_buffer = ft.DFT(buffer_);
    std::vector<Complex> dft_multiplied;
    int q = 0;
    while (q < dft_buffer.size()) {
      dft_multiplied.push_back(dft_ir[q] * dft_buffer[q]);
      q++;
    }

    buffer_ = ft.IDFT(dft_multiplied);
  }
  return buffer_;
}

std::vector<short> Effects::apply(std::vector<short> &buffer, size_t maxLen) {
  FourierTransform ft;
  std::vector<short> output;
  std::vector<short> buffer_ = buffer;
  for (int i = 0; i < this->firs.size(); i++) {
    auto impulse = this->firs[i].getIR();
    size_t paddedSize = buffer_.size() + impulse.size() - 1;
    if (paddedSize > maxLen) {
      paddedSize = maxLen;
    }
    size_t nearestPowerOfTwo = std::pow(2, std::ceil(std::log2(paddedSize)));

    // Pad waveData and impulse response to nearestPowerOfTwo
    while (buffer_.size() < nearestPowerOfTwo) {
      buffer_.push_back(0);
    }

    while (impulse.size() < nearestPowerOfTwo) {
      impulse.push_back(0);
    }

    std::vector<Complex> dft_ir = ft.DFT(impulse);
    std::vector<Complex> dft_buffer = ft.DFT(buffer_);
    std::vector<Complex> dft_multiplied;
    int q = 0;
    while (q < dft_buffer.size()) {
      dft_multiplied.push_back(dft_ir[q] * dft_buffer[q]);
      q++;
    }

    buffer_ = ft.IDFT(dft_multiplied);
  }
  return buffer_;
}
