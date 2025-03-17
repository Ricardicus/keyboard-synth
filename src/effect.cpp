#include <algorithm>
#include <complex>
#include <vector>

#include "dft.hpp"
#include "effect.hpp"

using Complex = std::complex<double>;

std::vector<Complex> detuneSpectrum(const std::vector<Complex> &spectrum,
                                    double pitchCents) {
  double factor = std::pow(2.0, pitchCents / 100.0);
  int n = spectrum.size();
  std::vector<Complex> detuned(n, Complex(0.0, 0.0));

  for (int i = 0; i < n; i++) {
    double srcIndex = i / factor;
    int index0 = static_cast<int>(std::floor(srcIndex));
    int index1 = index0 + 1;
    double frac = srcIndex - index0;

    if (index1 < n) {
      detuned[i] = (1.0 - frac) * spectrum[index0] + frac * spectrum[index1];
    } else if (index0 < n) {
      detuned[i] = spectrum[index0];
    }
  }
  return detuned;
}

std::vector<short> Effect::apply(std::vector<short> &buffer) {
  switch (this->effectType) {
  case EffectType::Fir: {
    return this->apply_fir(buffer);
  }
  case EffectType::Chorus: {
    return this->apply_chorus(buffer);
  }
  }
  return {};
}

std::vector<short> Effect::apply(std::vector<short> &buffer, size_t maxLen) {
  switch (this->effectType) {
  case EffectType::Fir: {
    return this->apply_fir(buffer, maxLen);
  }
  case EffectType::Chorus: {
    return this->apply_chorus(buffer);
  }
  }
  return {};
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
  std::vector<Complex> processedFT = FourierTransform::DFT(buffer, false);
  std::vector<std::vector<Complex>> voices;

  std::complex<double> phaseIncrement =
      std::polar(1.0, 2.0 * M_PI * this->chorusConfig.delay);

  int sampleRate = this->sampleRate;

  // Create each voice with an increasing offset and phase
  for (int i = 0; i < this->chorusConfig.numVoices; i++) {
    std::vector<Complex> voiceBuffer =
        detuneSpectrum(processedFT, (i + 1) * this->chorusConfig.depth);
    // Use std::pow to get the appropriate phase factor for this voice
    std::complex<double> phaseFactor = std::pow(phaseIncrement, i);

    for (int n = 0; n < static_cast<int>(voiceBuffer.size()); n++) {
      voiceBuffer[n] = voiceBuffer[n] * phaseFactor;
    }
    voices.push_back(voiceBuffer);
  }

  // Mix the original signal with the voices.
  for (int i = 0; i < static_cast<int>(processedFT.size()); i++) {
    std::complex<double> res =
        processedFT[i] / static_cast<double>(this->chorusConfig.numVoices);
    for (int v = 0; v < static_cast<int>(voices.size()); v++) {
      res +=
          (voices[v][i] / static_cast<double>(this->chorusConfig.numVoices)) *
          0.5;
    }
    processedFT[i] = res;
  }

  return FourierTransform::IDFT(processedFT);
}
