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

template <>
std::vector<short> Effect<short>::apply_fir(const std::vector<short> &buffer) {
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

template <>
std::vector<short> Effect<short>::apply_fir(const std::vector<short> &buffer,
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

template <>
std::vector<short>
Effect<short>::apply_chorus(const std::vector<short> &buffer) {
  if (auto conf = std::get_if<Effect::ChorusConfig>(&this->config)) {
    int numSamples = buffer.size();
    std::vector<Complex> processedFT = FourierTransform::DFT(buffer, false);
    std::vector<std::vector<Complex>> voices;

    std::complex<double> phaseIncrement =
        std::polar(1.0, 2.0 * M_PI * conf->delay);

    int sampleRate = this->sampleRate;

    // Create each voice with an increasing offset and phase
    for (int i = 0; i < conf->numVoices; i++) {
      std::vector<Complex> voiceBuffer =
          detuneSpectrum(processedFT, (i + 1) * conf->depth);
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
          processedFT[i] / static_cast<double>(conf->numVoices);
      for (int v = 0; v < static_cast<int>(voices.size()); v++) {
        res += (voices[v][i] / static_cast<double>(conf->numVoices)) * 0.5;
      }
      processedFT[i] = res;
    }

    return FourierTransform::IDFT(processedFT);
  } else {
    // Nothing happens
    return buffer;
  }
}

template <>
std::vector<short> Effect<short>::apply_iir(const std::vector<short> &buffer) {
  std::vector<short> output;
  std::vector<short> buffer_ = buffer;
  for (int i = 0; i < this->iirs.size(); i++) {
    this->iirs[i].clear();
    short output;
    for (int n = 0; n < buffer.size(); n++) {
      short input = buffer[n];
      output = this->iirs[i].process(input);
      buffer_[n] = output;
    }
  }
  return buffer_;
}

template <> float EchoEffect<float>::process(float inputSample) {
  size_t readIndex = (writeIndex + 1) % delaySamples;
  float delayedSample = buffer[readIndex];
  float wet = inputSample + delayedSample;
  float outputSample = (1.0f - mix) * inputSample + mix * wet;

  buffer[writeIndex] = inputSample + delayedSample * feedback;

  writeIndex = (writeIndex + 1) % delaySamples;

  return outputSample;
}

template <> short EchoEffect<short>::process(short inputSample) {
  size_t readIndex = (writeIndex + 1) % delaySamples;
  float delayedSample = static_cast<float>(buffer[readIndex]);
  float wet = inputSample + delayedSample;
  float outputSample = (1.0f - mix) * inputSample + mix * wet;

  buffer[writeIndex] =
      static_cast<short>(inputSample + delayedSample * feedback);

  writeIndex = (writeIndex + 1) % delaySamples;

  return outputSample;
}

template <>
std::vector<short> Effect<short>::apply(const std::vector<short> &buffer) {
  switch (this->effectType) {
  case Type::Fir: {
    return this->apply_fir(buffer);
  }
  case Type::Iir: {
    return this->apply_iir(buffer);
  }
  case Type::Chorus: {
    return this->apply_chorus(buffer);
  }
  default:
    break;
  }
  return buffer;
}

template <>
std::vector<short> Effect<short>::apply(const std::vector<short> &buffer,
                                        size_t maxLen) {
  switch (this->effectType) {
  case Type::Fir: {
    return this->apply_fir(buffer, maxLen);
  }
  case Type::Iir: {
    return this->apply_iir(buffer);
  }
  case Type::Chorus: {
    return this->apply_chorus(buffer);
  }
  default:
    break;
  }
  return buffer;
}

Effect<float> PresetEffects::syntheticReverb(float dry, float wet) {
  // Adding Reverb
  std::vector<Effect<float>> effects_sum;
  std::vector<std::vector<Effect<float>>> sum_pipes;
  std::vector<Effect<float>> pipe_effects;

  EchoEffect<float> e1{0.1, 0.5, 1.0,
                       static_cast<float>(Config::instance().getSampleRate())};
  EchoEffect<float> e2{0.12, 0.5, 1.0,
                       static_cast<float>(Config::instance().getSampleRate())};
  EchoEffect<float> e3{0.17, 0.7, 1.0,
                       static_cast<float>(Config::instance().getSampleRate())};

  Effect<float> ee1, ee2, ee3, se, ap1e, ap2e, ap3e, ap4e;
  ee1.effectType = Effect<float>::Type::Echo;
  ee1.config = e1;
  ee2.effectType = Effect<float>::Type::Echo;
  ee2.config = e2;
  ee3.effectType = Effect<float>::Type::Echo;
  ee3.config = e3;

  effects_sum.push_back(ee1);
  effects_sum.push_back(ee2);
  effects_sum.push_back(ee3);

  Adder<float> sum(effects_sum);
  se.effectType = Effect<float>::Type::Sum;
  se.config = sum;

  sum_pipes.push_back({se});
  sum_pipes.push_back({});
  std::vector<float> sum_pipe_mix{1.0, 0.1};
  Piper<float> sum_piper{sum_pipes, sum_pipe_mix};
  Effect<float> initial_sum;
  initial_sum.effectType = Effect<float>::Type::Pipe;
  initial_sum.config = sum_piper;

  // Add two all pass
  // Short early‑diffusion stage
  AllPassEffect<float> ap1(347, 0.1f); // ~7 ms @48 kHz
  AllPassEffect<float> ap2(113, 0.1f);

  // Late tail smoothing
  AllPassEffect<float> ap3(672, 0.1f); // ~14 ms
  AllPassEffect<float> ap4(908, 0.1f);

  ap1e.effectType = Effect<float>::Type::AllPass;
  ap2e.effectType = Effect<float>::Type::AllPass;
  ap3e.effectType = Effect<float>::Type::AllPass;
  ap4e.effectType = Effect<float>::Type::AllPass;

  ap1e.config = ap1;
  ap2e.config = ap2;
  ap3e.config = ap3;
  ap4e.config = ap4;

  pipe_effects.push_back(initial_sum);
  pipe_effects.push_back(ap1e);
  pipe_effects.push_back(ap2e);
  pipe_effects.push_back(ap3e);
  pipe_effects.push_back(ap4e);

  Effect<float> reverb;
  std::vector<std::vector<Effect<float>>> reverb_pipes;
  reverb_pipes.push_back(pipe_effects);
  reverb_pipes.push_back({});
  std::vector<float> pipe_mix{wet, dry};

  Piper<float> piper{reverb_pipes, pipe_mix};
  reverb.effectType = Effect<float>::Type::Pipe;
  reverb.config = piper;

  return reverb;
}
