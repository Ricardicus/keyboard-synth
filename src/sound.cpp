#include "sound.hpp"
#include "note.hpp"
#include <cmath>
#include <functional>
#include <ncurses.h>
#include <thread>
#include <type_traits>
#include <variant>

const float PI = 3.14159265358979323846f;

std::string Sound::typeOfWave(Sound::WaveForm form) {
  switch (form) {
  case Sine:
    return "sine";
  case Triangular:
    return "triangular";
  case Square:
    return "square";
  case Saw:
    return "saw";
  case WhiteNoise:
    return "white_noise";
  case WaveFile:
    return "file";
  }
  return "none";
}

std::vector<short> Sound::generateWave(Sound::WaveForm form, Note &note,
                                       ADSR &adsr,
                                       std::vector<Effect<short>> &effects) {
  std::vector<short> result;
  switch (form) {
  case Sound::WaveForm::Sine:
    result = Sound::generateSineWave(note, adsr, effects);
    break;
  case Sound::WaveForm::Triangular:
    result = Sound::generateTriangularWave(note, adsr, effects);
    break;
  case Sound::WaveForm::Square:
    result = Sound::generateSquareWave(note, adsr, effects);
    break;
  case Sound::WaveForm::Saw:
    result = Sound::generateSawWave(note, adsr, effects);
    break;
  case Sound::WaveForm::WhiteNoise:
    result = Sound::generateWhiteNoiseWave(note, adsr, effects);
    break;
  case Sound::WaveForm::WaveFile:
    break;
  }
  return result;
}

float Sound::sinus(float f) { return sin(f); }

float Sound::square(float f, float factor) {
  f = fmod(f, 2.0 * PI);
  if (f < PI * factor) {
    return 1.0;
  }
  return -1.0;
}

float Sound::triangular(float f) {
  f = fmod(f, 2.0 * PI);
  if (f < PI / 2.0) {
    return f / (PI / 2.0);
  } else if (f < PI + PI / 2) {
    return 1 - (f - PI / 2) / (PI / 2);
  } else {
    return (f - (PI + PI / 2)) / (PI / 2) - 1;
  }
  return 1.0;
}

float Sound::saw(float f) {
  f = fmod(f, 2.0 * PI);
  if (f < PI / 2.0) {
    return f / (PI / 2.0);
  } else if (f < PI + PI / 2) {
    return (f - PI / 2) / (PI / 2) - 1;
  } else {
    return (f - (PI + PI / 2)) / (PI / 2) - 1;
  }
  return 1.0;
}

float Sound::white_noise(float f) {
  return (float)(rand() % 2001 - 1000) /
         1000.0; // Random value between -1.0 and 1.0
}

using UnaryOp = float (*)(float);
using BinaryOp = float (*)(float, float);
using Operation = std::variant<UnaryOp, BinaryOp>;
// Dispatcher function
float generateWaveBit(float phase, float duty, Operation op) {
  return std::visit(
      [phase, duty](auto &&func) -> float {
        using F = std::decay_t<decltype(func)>;
        if constexpr (std::is_same_v<F, UnaryOp>) {
          return func(phase); // Ignore y
        } else if constexpr (std::is_same_v<F, BinaryOp>) {
          return func(phase, duty);
        } else {
          return -1.0;
        }
      },
      op);
}

template <typename T>
void applyEffects(float t, float &phase, float &duty, short &envelope,
                  std::vector<Effect<T>> &effects) {
  for (int e = 0; e < effects.size(); e++) {
    if (auto conf = std::get_if<typename Effect<T>::VibratoConfig>(
            &effects[e].config)) {
      phase += conf->depth * sin(2.0f * PI * conf->frequency * t);
    }
    if (auto conf = std::get_if<typename Effect<T>::DutyCycleConfig>(
            &effects[e].config)) {
      duty += conf->depth * sin(2.0f * PI * conf->frequency * t);
    }
    if (auto conf = std::get_if<typename Effect<T>::TremoloConfig>(
            &effects[e].config)) {
      envelope = conf->depth * sin(2.0f * PI * conf->frequency * t) * envelope +
                 (1.0 - conf->depth) * envelope;
    }
  }
}

template <>
float Sound::applyPostEffects(float sample,
                              std::vector<Effect<float>> &effects) {
  float result = sample;
  for (int e = 0; e < effects.size(); e++) {
    if (auto echo = std::get_if<EchoEffect<float>>(&effects[e].config)) {
      result = echo->process(result);
    } else if (auto allpass =
                   std::get_if<AllPassEffect<float>>(&effects[e].config)) {
      result = allpass->process(result);
    } else if (auto sum = std::get_if<Adder<float>>(&effects[e].config)) {
      result = sum->process(result);
    } else if (auto pipe = std::get_if<Piper<float>>(&effects[e].config)) {
      result = pipe->process(result);
    }
  }
  return result;
}

template <>
short Sound::applyPostEffects(short sample,
                              std::vector<Effect<short>> &effects) {
  short result = sample;
  for (int e = 0; e < effects.size(); e++) {
    if (auto echo = std::get_if<EchoEffect<short>>(&effects[e].config)) {
      result = echo->process(result);
    } else if (auto allpass =
                   std::get_if<AllPassEffect<short>>(&effects[e].config)) {
      result = allpass->process(result);
    }
  }
  return result;
}

std::vector<short>
Sound::generateSineWave(Note &note, ADSR &adsr,
                        std::vector<Effect<short>> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    float t = i * deltaT;
    float phase = 2.0f * PI * note.frequency * t;
    float duty = 0.0;
    short envelope = adsr.response(i);
    applyEffects(t, phase, duty, envelope, effects);
    short sample = static_cast<short>(
        envelope * generateWaveBit(phase, duty, Sound::sinus));
    samples[i] = applyPostEffects(sample, effects);
  }

  return samples;
}

std::vector<short>
Sound::generateSquareWave(Note &note, ADSR &adsr,
                          std::vector<Effect<short>> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    float t = i * deltaT;
    float phase = 2.0f * PI * note.frequency * t;
    float duty = 1.0;
    short envelope = adsr.response(i);
    applyEffects(t, phase, duty, envelope, effects);
    short sample = static_cast<short>(
        envelope *
        generateWaveBit(phase, duty, static_cast<BinaryOp>(&Sound::square)));
    samples[i] = applyPostEffects(sample, effects);
  }

  return samples;
}

std::vector<short>
Sound::generateTriangularWave(Note &note, ADSR &adsr,
                              std::vector<Effect<short>> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    float t = i * deltaT;
    float phase = 2.0f * PI * note.frequency * t;
    float duty = 0.0;
    short envelope = adsr.response(i);
    applyEffects(t, phase, duty, envelope, effects);
    short sample = static_cast<short>(
        envelope * generateWaveBit(phase, duty, Sound::triangular));
    samples[i] = applyPostEffects(sample, effects);
  }

  return samples;
}

std::vector<short> Sound::generateSawWave(Note &note, ADSR &adsr,
                                          std::vector<Effect<short>> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    float t = i * deltaT;
    float phase = 2.0f * PI * note.frequency * t;
    float duty = 0.0;
    short envelope = adsr.response(i);
    applyEffects(t, phase, duty, envelope, effects);
    samples[i] = envelope * generateWaveBit(phase, duty, Sound::saw);
  }

  return samples;
}

std::vector<short>
Sound::generateWhiteNoiseWave(Note &note, ADSR &adsr,
                              std::vector<Effect<short>> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    float t = i * deltaT;
    float phase = 2.0f * PI * note.frequency * t;
    float duty = 0.0;
    short envelope = adsr.response(i);
    applyEffects(t, phase, duty, envelope, effects);
    samples[i] = envelope * generateWaveBit(phase, duty, Sound::white_noise);
  }

  return samples;
}

template <> float Sound::Rank<float>::generateRankSample() {
  float val = 0;
  for (const Pipe &pipe : this->pipes) {
    Note note = pipe.first;
    Sound::WaveForm form = pipe.second;
    float deltaT = 1.0f / note.sampleRate;
    float addition;
    float frequency = note.frequency;
    if (note.frequencyAltered > 0) {
      frequency = note.frequencyAltered;
    }
    float t = this->generatorIndex * deltaT;
    float phase = 2.0f * PI * frequency * t;
    float duty = 1.0;
    short envelope = adsr.amplitude *
                     note.volume; // TODO: adsr.response(this->generatorIndex);
    applyEffects(t, phase, duty, envelope, effects);

    switch (form) {
    case Sound::WaveForm::Sine: {
      addition = generateWaveBit(phase, duty, Sound::sinus);
      break;
    }
    case Sound::WaveForm::Triangular: {
      addition = generateWaveBit(phase, duty, Sound::triangular);
      break;
    }
    case Sound::WaveForm::Square: {
      addition =
          generateWaveBit(phase, duty, static_cast<BinaryOp>(&Sound::square));
      break;
    }
    case Sound::WaveForm::Saw: {
      addition = generateWaveBit(phase, duty, Sound::saw);
      break;
    }
    case Sound::WaveForm::WhiteNoise: {
      addition = generateWaveBit(phase, duty, Sound::white_noise);
      break;
    }
    case Sound::WaveForm::WaveFile:
      break;
    }

    val += (static_cast<float>(envelope) / adsr.amplitude) * addition;
  }

  this->generatorIndex++;
  return val;
}

template <> float Sound::Rank<float>::generateRankSampleIndex(int index) {
  this->generatorIndex = index;
  return this->generateRankSample();
}

std::vector<short> Sound::generateWave(Rank<short> &rank) {
  int sampleCount = 0;
  for (const auto &pipe : rank.pipes) {
    if (pipe.first.length > sampleCount) {
      sampleCount = pipe.first.length;
    }
  }
  std::vector<short> samples(sampleCount);

  for (int i = 0; i < sampleCount; i++) {
    short val = 0;
    for (const Pipe &pipe : rank.pipes) {
      Note note = pipe.first;
      Sound::WaveForm form = pipe.second;
      float deltaT = 1.0f / note.sampleRate;
      float t = i * deltaT;
      float phase = 2.0f * PI * note.frequency * t;
      float duty = 1.0;
      short envelope = rank.adsr.response(i);
      applyEffects(t, phase, duty, envelope, rank.effects);
      short addition = 0.0f;
      switch (form) {
      case Sound::WaveForm::Sine: {
        addition = envelope * generateWaveBit(phase, duty, Sound::sinus);
        break;
      }
      case Sound::WaveForm::Triangular: {
        addition = envelope * generateWaveBit(phase, duty, Sound::triangular);
        break;
      }
      case Sound::WaveForm::Square: {
        addition =
            envelope *
            generateWaveBit(phase, duty, static_cast<BinaryOp>(&Sound::square));
        break;
      }
      case Sound::WaveForm::Saw: {
        addition = envelope * generateWaveBit(phase, duty, Sound::saw);
        break;
      }
      case Sound::WaveForm::WhiteNoise: {
        addition = envelope * generateWaveBit(phase, duty, Sound::white_noise);
        break;
      }
      case Sound::WaveForm::WaveFile:
        break;
      }

      val += addition;
    }
    samples[i] = val;
  }

  return samples;
} // namespace Sound
