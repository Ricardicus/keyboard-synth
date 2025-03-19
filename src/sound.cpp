#include "sound.hpp"
#include "note.hpp"
#include <cmath>
#include <ncurses.h>
#include <thread>

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
  case WaveFile:
    return "file";
  }
  return "none";
}

std::vector<short> Sound::generateWave(Sound::WaveForm form, Note &note,
                                       ADSR &adsr,
                                       std::vector<Effect> &effects) {
  switch (form) {
  case Sound::WaveForm::Sine:
    return Sound::generateSineWave(note, adsr, effects);
  case Sound::WaveForm::Triangular:
    return Sound::generateTriangularWave(note, adsr, effects);
  case Sound::WaveForm::Square:
    return Sound::generateSquareWave(note, adsr, effects);
  case Sound::WaveForm::Saw:
    return Sound::generateSawWave(note, adsr, effects);
  case Sound::WaveForm::WaveFile:
    break;
  }

  return {};
}

static float sinus(float f) { return sin(f); }

static float square(float f) {
  f = fmod(f, 2.0 * PI);
  if (f < PI) {
    return 1.0;
  }
  return -1.0;
}

static float square(float f, float factor) {
  f = fmod(f, 2.0 * PI);
  if (f < PI * factor) {
    return 1.0;
  }
  return -1.0;
}

static float triangular(float f) {
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

static float saw(float f) {
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

std::vector<short> Sound::generateSineWave(Note &note, ADSR &adsr,
                                           std::vector<Effect> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           sinus(2.0f * PI * note.frequency * i * deltaT));
    if (effects.size() > 0) {
      for (int e = 0; e < effects.size(); e++) {
        switch (effects[e].effectType) {
        case Effect::Type::Vibrato: {
          samples[i] = static_cast<short>(
              adsr.response(i) * note.volume *
              sinus(2.0f * PI * note.frequency * i * deltaT +
                    effects[e].vibratoConfig.depth *
                        sin(2.0f * PI * effects[e].vibratoConfig.frequency * i *
                            deltaT)));
        }
        default:
          break;
        }
      }
    }
  }

  return samples;
}

std::vector<short> Sound::generateSquareWave(Note &note, ADSR &adsr,
                                             std::vector<Effect> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           square(2.0 * PI * note.frequency * i * deltaT));
    if (effects.size() > 0) {
      for (int e = 0; e < effects.size(); e++) {
        switch (effects[e].effectType) {
        case Effect::Type::Vibrato: {
          samples[i] = static_cast<short>(
              adsr.response(i) * note.volume *
              square(2.0f * PI * note.frequency * i * deltaT +
                     effects[e].vibratoConfig.depth *
                         sin(2.0f * PI * effects[e].vibratoConfig.frequency *
                             i * deltaT)));

          break;
        }
        case Effect::Type::DutyCycle: {
          samples[i] = static_cast<short>(
              adsr.response(i) * note.volume *
              square(2.0f * PI * note.frequency * i * deltaT,
                     effects[e].dutyCycleConfig.depth *
                         (0.5 + 0.5 * sin(2.0f * PI *
                                          effects[e].dutyCycleConfig.frequency *
                                          i * deltaT))));

          break;
        }
        default:
          break;
        }
      }
    }
  }

  return samples;
}

std::vector<short> Sound::generateTriangularWave(Note &note, ADSR &adsr,
                                                 std::vector<Effect> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           triangular(2.0f * PI * note.frequency * i * deltaT));
    if (effects.size() > 0) {
      for (int e = 0; e < effects.size(); e++) {
        switch (effects[e].effectType) {
        case Effect::Type::Vibrato: {
          samples[i] = static_cast<short>(
              adsr.response(i) * note.volume *
              triangular(
                  2.0f * PI * note.frequency * i * deltaT +
                  effects[e].vibratoConfig.depth *
                      sin(2.0f * PI * effects[e].vibratoConfig.frequency * i *
                          deltaT)));
        }
        default:
          break;
        }
      }
    }
  }

  return samples;
}

std::vector<short> Sound::generateSawWave(Note &note, ADSR &adsr,
                                          std::vector<Effect> &effects) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           saw(2.0f * PI * note.frequency * i * deltaT));
    if (effects.size() > 0) {
      for (int e = 0; e < effects.size(); e++) {
        switch (effects[e].effectType) {
        case Effect::Type::Vibrato: {
          samples[i] = static_cast<short>(
              adsr.response(i) * note.volume *
              saw(2.0f * PI * note.frequency * i * deltaT +
                  effects[e].vibratoConfig.depth *
                      sin(2.0f * PI * effects[e].vibratoConfig.frequency * i *
                          deltaT)));
        }
        default:
          break;
        }
      }
    }
  }

  return samples;
}

std::vector<short> Sound::generateWave(Rank &rank) {
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

      switch (form) {
      case Sound::WaveForm::Sine: {
        short addition =
            static_cast<short>(rank.adsr.response(i) * note.volume *
                               sinus(2.0f * PI * note.frequency * i * deltaT));
        if (rank.effects.size() > 0) {
          for (int e = 0; e < rank.effects.size(); e++) {
            switch (rank.effects[e].effectType) {
            case Effect::Type::Vibrato: {
              addition = static_cast<short>(
                  rank.adsr.response(i) * note.volume *
                  sin(2.0f * PI * note.frequency * i * deltaT +
                      rank.effects[e].vibratoConfig.depth *
                          sin(2.0f * PI *
                              rank.effects[e].vibratoConfig.frequency * i *
                              deltaT)));
            }
            default:
              break;
            }
          }
        }
        val += addition;
        break;
      }
      case Sound::WaveForm::Triangular: {
        short addition = static_cast<short>(
            rank.adsr.response(i) * note.volume *
            triangular(2.0f * PI * note.frequency * i * deltaT));
        if (rank.effects.size() > 0) {
          for (int e = 0; e < rank.effects.size(); e++) {
            switch (rank.effects[e].effectType) {
            case Effect::Type::Vibrato: {
              addition = static_cast<short>(
                  rank.adsr.response(i) * note.volume *
                  triangular(2.0f * PI * note.frequency * i * deltaT +
                             rank.effects[e].vibratoConfig.depth *
                                 sin(2.0f * PI *
                                     rank.effects[e].vibratoConfig.frequency *
                                     i * deltaT)));
            }
            default:
              break;
            }
          }
        }
        val += addition;
        break;
      }
      case Sound::WaveForm::Square: {
        short addition =
            static_cast<short>(rank.adsr.response(i) * note.volume *
                               square(2.0f * PI * note.frequency * i * deltaT));
        if (rank.effects.size() > 0) {
          for (int e = 0; e < rank.effects.size(); e++) {
            switch (rank.effects[e].effectType) {
            case Effect::Type::Vibrato: {
              addition = static_cast<short>(
                  rank.adsr.response(i) * note.volume *
                  square(2.0f * PI * note.frequency * i * deltaT,
                         rank.effects[e].vibratoConfig.depth *
                             sin(2.0f * PI *
                                 rank.effects[e].vibratoConfig.frequency * i *
                                 deltaT)));
              break;
            }
            case Effect::Type::DutyCycle: {
              addition = static_cast<short>(
                  rank.adsr.response(i) * note.volume *
                  square(2.0f * PI * note.frequency * i * deltaT +
                         rank.effects[e].dutyCycleConfig.depth *
                             sin(2.0f * PI *
                                 rank.effects[e].dutyCycleConfig.frequency * i *
                                 deltaT)));
              break;
            }
            default:
              break;
            }
          }
        }
        val += addition;
        break;
      }
      case Sound::WaveForm::Saw: {
        short addition =
            static_cast<short>(rank.adsr.response(i) * note.volume *
                               saw(2.0f * PI * note.frequency * i * deltaT));
        if (rank.effects.size() > 0) {
          for (int e = 0; e < rank.effects.size(); e++) {
            switch (rank.effects[e].effectType) {
            case Effect::Type::Vibrato: {
              addition = static_cast<short>(
                  rank.adsr.response(i) * note.volume *
                  saw(2.0f * PI * note.frequency * i * deltaT +
                      rank.effects[e].vibratoConfig.depth *
                          sin(2.0f * PI *
                              rank.effects[e].vibratoConfig.frequency * i *
                              deltaT)));
            }
            default:
              break;
            }
          }
        }
        val += addition;
        break;
      }
      case Sound::WaveForm::WaveFile:
        break;
      }
    }
    samples[i] = val;
  }

  return samples;
}

Sound::Rank Sound::Rank::superSaw(float frequency, int length, int sampleRate) {
  Rank rank;

  float detune = 0.5;
  float detune_cents[] = {-12.0, -8.0, -4.0, 0.0, 4.0, 8.0, 12.0};
  const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  for (int i = 0; i < num_oscillators; i++) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 1.0 / num_oscillators;
    Pipe pipe(note, Sound::WaveForm::Saw);
    rank.addPipe(pipe);
  }

  return rank;
}

Sound::Rank Sound::Rank::fatTriangle(float frequency, int length,
                                     int sampleRate) {
  Rank rank;
  float detune = 0.3f; // Subtle detuning
  float detune_cents[] = {-10.0f, -5.0f, 0.0f, 5.0f, 10.0f};
  const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  for (int i = 0; i < num_oscillators; ++i) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 0.8f / num_oscillators; // Slightly lower amplitude for warmth
    Pipe pipe(note, Sound::WaveForm::Triangular);
    rank.addPipe(pipe);
  }

  // Optional: Add vibrato for extra depth
  Effect effect;
  effect.effectType = Effect::Type::Vibrato;
  effect.vibratoConfig.depth = 0.015;
  effect.vibratoConfig.frequency = 4.0;
  rank.effects.push_back(effect); // 1.5% depth, 4 Hz

  return rank;
}

Sound::Rank Sound::Rank::pulseSquare(float frequency, int length,
                                     int sampleRate) {
  Rank rank;
  float detune = 0.2f;
  float detune_cents[] = {-6.0f, 0.0f, 6.0f};
  const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  for (int i = 0; i < num_oscillators; ++i) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 1.0f / num_oscillators;
    Pipe pipe(note, Sound::WaveForm::Square);
    rank.addPipe(pipe);
  }

  // Add vibrato for a retro wiggle
  Effect effect;
  effect.effectType = Effect::Type::Vibrato;
  effect.vibratoConfig.depth = 0.03f;    // 3% depth
  effect.vibratoConfig.frequency = 6.0f; // 6 Hz
  rank.effects.push_back(effect);

  Effect pwmEffect;
  pwmEffect.effectType = Effect::Type::DutyCycle;
  pwmEffect.dutyCycleConfig.frequency = 3.0f;
  pwmEffect.dutyCycleConfig.depth = 0.4f;
  rank.effects.push_back(pwmEffect);

  return rank;
}

Sound::Rank Sound::Rank::sineSawDrone(float frequency, int length,
                                      int sampleRate) {
  Rank rank;
  float detune = 0.4f;
  float detune_cents[] = {-8.0f, 0.0f, 8.0f};

  // Core sine wave
  Note sineNote(frequency, length, sampleRate);
  sineNote.volume = 0.5f; // Louder base tone
  Pipe sinePipe(sineNote, Sound::WaveForm::Sine);
  rank.addPipe(sinePipe);

  // Detuned saws
  const int num_saws = sizeof(detune_cents) / sizeof(detune_cents[0]);
  for (int i = 0; i < num_saws; ++i) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 0.3f / num_saws; // Softer saws
    Pipe pipe(note, Sound::WaveForm::Saw);
    rank.addPipe(pipe);
  }

  // Slow vibrato for evolving feel
  Effect effect;
  effect.effectType = Effect::Type::Vibrato;
  effect.vibratoConfig.depth = 0.01f;    // 1% depth
  effect.vibratoConfig.frequency = 2.5f; // 2.5 Hz
  rank.effects.push_back(effect);

  return rank;
}

Sound::Rank Sound::Rank::superSawWithSub(float frequency, int length,
                                         int sampleRate) {
  Rank rank;
  float detune = 0.5f;
  float detune_cents[] = {-12.0f, -8.0f, -4.0f, 0.0f, 4.0f, 8.0f, 12.0f};
  const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  // Supersaw oscillators
  for (int i = 0; i < num_oscillators; ++i) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 0.8f / num_oscillators;
    Pipe pipe(note, Sound::WaveForm::Saw);
    rank.addPipe(pipe);
  }

  // Sub-oscillator (one octave down)
  Note subNote(frequency / 2.0f, length, sampleRate);
  subNote.volume = 0.4f; // Strong but not overpowering
  Pipe subPipe(subNote, Sound::WaveForm::Sine);
  rank.addPipe(subPipe);

  return rank; // No vibrato here, but you could add it if desired
}

Sound::Rank Sound::Rank::glitchMix(float frequency, int length,
                                   int sampleRate) {
  Rank rank;
  float detune = 0.7f;
  float detune_cents[] = {-15.0f, -7.0f, 0.0f, 7.0f, 15.0f};
  const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  for (int i = 0; i < num_oscillators; ++i) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 1.0f / num_oscillators;
    Pipe pipe(note,
              (i % 2 == 0) ? Sound::WaveForm::Square : Sound::WaveForm::Saw);
    rank.addPipe(pipe);
  }

  // Fast vibrato for glitchy vibe
  Effect effect;
  effect.effectType = Effect::Type::Vibrato;
  effect.vibratoConfig.depth = 0.05f;     // 5% depth
  effect.vibratoConfig.frequency = 10.0f; // 10 Hz
  rank.effects.push_back(effect);

  return rank;
}

Sound::Rank Sound::Rank::lushPad(float frequency, int length, int sampleRate) {
  Rank rank;
  float detune_cents[] = {-8.0f, -4.0f, 0.0f, 4.0f, 8.0f};
  int numOscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  for (int i = 0; i < numOscillators; ++i) {
    float detunedFreq = frequency * powf(2.0f, detune_cents[i] / 1200.0f);
    Note note(detunedFreq, length, sampleRate);
    note.volume = 0.7f / numOscillators;
    Pipe pipe(note, (i % 2 == 0) ? Sound::WaveForm::Triangular
                                 : Sound::WaveForm::Sine);
    rank.addPipe(pipe);
  }

  Effect chorusEffect;
  chorusEffect.effectType = Effect::Type::Chorus;
  chorusEffect.chorusConfig = {0.03f, 0.2f, 3};
  rank.effects.push_back(chorusEffect);

  return rank;
}

Sound::Rank Sound::Rank::retroLead(float frequency, int length,
                                   int sampleRate) {
  Rank rank;

  Note squareNote(frequency, length, sampleRate);
  squareNote.volume = 0.8f;
  Pipe squarePipe(squareNote, Sound::WaveForm::Square);
  rank.addPipe(squarePipe);

  Effect pwmEffect;
  pwmEffect.effectType = Effect::Type::DutyCycle;
  pwmEffect.dutyCycleConfig.frequency = 5.0f;
  pwmEffect.dutyCycleConfig.depth = 0.4f;
  rank.effects.push_back(pwmEffect);

  return rank;
}

Sound::Rank Sound::Rank::organTone(float frequency, int length,
                                   int sampleRate) {
  Rank rank;

  Note mainNote(frequency, length, sampleRate);
  mainNote.volume = 0.6f;
  Pipe mainPipe(mainNote, Sound::WaveForm::Sine);
  rank.addPipe(mainPipe);

  // Add harmonics
  float harmonics[] = {frequency * 2.0f, frequency * 3.0f};
  for (float freq : harmonics) {
    Note harmonic(freq, length, sampleRate);
    harmonic.volume = 0.2f;
    Pipe harmonicPipe(harmonic, Sound::WaveForm::Sine);
    rank.addPipe(harmonicPipe);
  }

  Effect tremoloEffect;
  tremoloEffect.effectType = Effect::Type::Vibrato;
  tremoloEffect.vibratoConfig = {5.0f, 0.01f}; // Tremolo at 5 Hz, subtle depth
  rank.effects.push_back(tremoloEffect);

  return rank;
}

Sound::Rank Sound::Rank::bassGrowl(float frequency, int length,
                                   int sampleRate) {
  Rank rank;

  Note sawNote(frequency, length, sampleRate);
  sawNote.volume = 0.6f;
  Pipe sawPipe(sawNote, Sound::WaveForm::Saw);
  rank.addPipe(sawPipe);

  Note subBass(frequency / 2.0f, length, sampleRate);
  subBass.volume = 0.4f;
  Pipe subPipe(subBass, Sound::WaveForm::Sine);
  rank.addPipe(subPipe);

  // (Add distortion effect via FIR/IIR if available in your effect stack)

  return rank;
}

Sound::Rank Sound::Rank::ambientDrone(float frequency, int length,
                                      int sampleRate) {
  Rank rank;

  float detune_cents[] = {-5.0f, 0.0f, 5.0f};
  for (float cents : detune_cents) {
    float detunedFreq = frequency * powf(2.0f, cents / 1200.0f);
    Note note(detunedFreq, length, sampleRate);
    note.volume = 0.4f / 3;
    Pipe pipe(note, Sound::WaveForm::Sine);
    rank.addPipe(pipe);
  }

  Effect slowVibrato;
  slowVibrato.effectType = Effect::Type::Vibrato;
  slowVibrato.vibratoConfig = {0.8f, 0.01f}; // slow vibrato
  rank.effects.push_back(slowVibrato);

  return rank;
}

Sound::Rank Sound::Rank::synthStab(float frequency, int length,
                                   int sampleRate) {
  Rank rank;

  Note sawNote(frequency, length / 4, sampleRate); // very short envelope
  sawNote.volume = 0.8f;
  Pipe sawPipe(sawNote, Sound::WaveForm::Saw);
  rank.addPipe(sawPipe);

  Note triangleNote(frequency * 2, length / 4, sampleRate);
  triangleNote.volume = 0.5f;
  Pipe triPipe(triangleNote, Sound::WaveForm::Triangular);
  rank.addPipe(triPipe);

  return rank;
}

Sound::Rank Sound::Rank::glassBells(float frequency, int length,
                                    int sampleRate) {
  Rank rank;

  float detune_cents[] = {-3.0f, 0.0f, 3.0f};
  for (int i = 0; i < 3; ++i) {
    float detuned_freq = frequency * powf(2.0f, detune_cents[i] / 1200.0f);
    Note sineNote(detuned_freq, length, sampleRate);
    sineNote.volume = 0.5 / 3;
    Pipe sinePipe(sineNote, Sound::WaveForm::Sine);
    rank.addPipe(sinePipe);
  }

  return rank;
}
