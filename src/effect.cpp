#include <algorithm>
#include <complex>
#include <vector>

#include "dft.hpp"
#include "effect.hpp"

using Complex = std::complex<double>;

std::vector<short> Effect::apply(std::vector<short> &buffer) {
  switch (this->effectType) {
  case EffectType::FIR: {
    return this->apply_fir(buffer);
  }
  case EffectType::Chorus: {
    return this->apply_chorus(buffer);
  }
  }
}

std::vector<short> Effect::apply(std::vector<short> &buffer, size_t maxLen) {
  switch (this->effectType) {
  case EffectType::FIR: {
    return this->apply_fir(buffer, maxLen);
  }
  case EffectType::Chorus: {
    return this->apply_chorus(buffer);
  }
  }
}

std::vector<short> Effect::apply_fir(std::vector<short> &buffer) {
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
    std::vector<Complex> dft_ir =
        ft.DFT(impulse, this->firs[i].getNormalization());
    std::vector<Complex> dft_buffer = ft.DFT(buffer_, false);
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

std::vector<short> Effect::apply_fir(std::vector<short> &buffer,
                                     size_t maxLen) {
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
    std::vector<Complex> dft_ir =
        ft.DFT(impulse, this->firs[i].getNormalization());
    std::vector<Complex> dft_buffer = ft.DFT(buffer_, false);
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

std::vector<short> Effect::apply_chorus(std::vector<short> &buffer) {
  int numSamples = buffer.size();
  std::vector<short> processedBuffer = buffer; // Copy input for modification

  for (int i = 0; i < numSamples; i++) {
    // 🌀 Modulate delay using LFO
    float lfo = sinf(2.0f * M_PI * chorusConfig.lfoRate * (float)i /
                     sampleRate); // LFO modulation

    int mixedSample = buffer[i]; // Dry signal (original)

    for (int j = 0; j < chorusConfig.numVoices; j++) {
      // Calculate delay in samples
      float voiceDelayMs =
          (j + 1) * chorusConfig.depthMs / chorusConfig.numVoices;
      int delaySamples =
          (voiceDelayMs + lfo * chorusConfig.depthMs) * sampleRate / 1000.0f;
      int delayedIndex = i - delaySamples;

      if (delayedIndex >= 0) {
        mixedSample += processedBuffer[delayedIndex]; // Mix delayed voices
      }
    }

    // Prevent clipping
    processedBuffer[i] = static_cast<short>(
        std::clamp(mixedSample / chorusConfig.numVoices, -32768, 32767));
  }

  return processedBuffer;
}
