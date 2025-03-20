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
  case Sound::WaveForm::WaveFile:
    break;
  }

  if (effects.size() > 0) {
    float deltaT = 1.0f / note.sampleRate;
    for (int e = 0; e < effects.size(); e++) {
      switch (effects[e].effectType) {
      case Effect::Type::Tremolo: {
        for (int i = 0; i < result.size(); i++) {
          result[i] = static_cast<short>(
              (effects[e].tremoloConfig.depth *
               sin(effects[e].tremoloConfig.frequency * i * deltaT) *
               result[i]) +
              (1.0 - effects[e].tremoloConfig.depth) * result[i]);
        }
      }
      default:
        break;
      }
    }
  }

  return result;
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

static float white_noise(float f) {
  return (float)(rand() % 2001 - 1000) /
         1000.0; // Random value between -1.0 and 1.0
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
      short addition;

      switch (form) {
      case Sound::WaveForm::Sine: {
        addition =
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
              break;
            }
            default:
              break;
            }
          }
        }
        break;
      }
      case Sound::WaveForm::Triangular: {
        addition = static_cast<short>(
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
        break;
      }
      case Sound::WaveForm::Square: {
        addition =
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
        break;
      }
      case Sound::WaveForm::Saw: {
        addition =
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
        break;
      }
      case Sound::WaveForm::WaveFile:
        break;
      }

      if (rank.effects.size() > 0) {
        for (int e = 0; e < rank.effects.size(); e++) {
          switch (rank.effects[e].effectType) {
          case Effect::Type::Tremolo: {
            addition = static_cast<short>(
                (rank.effects[e].tremoloConfig.depth *
                 sin(rank.effects[e].tremoloConfig.frequency * i * deltaT) *
                 addition) +
                (1.0 - rank.effects[e].tremoloConfig.depth) * addition);
            break;
          }
          default:
            break;
          }
        }
      }

      val += addition;
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

  // Main sine wave
  Note mainNote(frequency, length, sampleRate);
  mainNote.volume = 0.5f;
  Pipe mainPipe(mainNote, Sound::WaveForm::Sine);
  rank.addPipe(mainPipe);

  // Harmonics with slight detuning
  float harmonics[] = {frequency * 2.0f, frequency * 3.0f, frequency * 4.0f};
  float detune_cents[] = {-2.0f, 0.0f, 2.0f};
  for (int i = 0; i < 3; ++i) {
    float detuned_freq = harmonics[i] * powf(2.0f, detune_cents[i] / 1200.0f);
    Note harmonic(detuned_freq, length, sampleRate);
    harmonic.volume = 0.15f;
    Pipe harmonicPipe(harmonic, Sound::WaveForm::Sine);
    rank.addPipe(harmonicPipe);
  }

  // Leslie-like tremolo (fast vibrato)
  Effect tremolo;
  tremolo.effectType = Effect::Type::Vibrato;
  tremolo.vibratoConfig = {0.05f, 7.0f}; // 5% depth, 7 Hz for spinning feel
  rank.effects.push_back(tremolo);

  // Add a subtle saw for grit
  Note sawNote(frequency * 1.01f, length, sampleRate); // Slight detune
  sawNote.volume = 0.03f;
  Pipe sawPipe(sawNote, Sound::WaveForm::Saw);
  rank.addPipe(sawPipe);

  return rank;
}

Sound::Rank Sound::Rank::bassGrowl(float frequency, int length,
                                   int sampleRate) {
  Rank rank;

  // Main saw wave with detuning
  float detune_cents[] = {-4.0f, 0.0f, 4.0f};
  for (float cents : detune_cents) {
    float detuned_freq = frequency * powf(2.0f, cents / 1200.0f);
    Note sawNote(detuned_freq, length, sampleRate);
    sawNote.volume = 0.3f / 3;
    Pipe sawPipe(sawNote, Sound::WaveForm::Saw);
    rank.addPipe(sawPipe);
  }

  // Sub-bass sine
  Note subBass(frequency / 2.0f, length, sampleRate);
  subBass.volume = 0.35f;
  Pipe subPipe(subBass, Sound::WaveForm::Sine);
  rank.addPipe(subPipe);

  // Wobbling square with duty cycle
  Note squareNote(frequency * 0.99f, length, sampleRate); // Slight detune
  squareNote.volume = 0.25f;
  Pipe squarePipe(squareNote, Sound::WaveForm::Square);
  rank.addPipe(squarePipe);

  // PWM effect for growl
  Effect pwm;
  pwm.effectType = Effect::Type::DutyCycle;
  pwm.dutyCycleConfig =
      Effect::DutyCycleConfig(3.0f, 0.45f); // 3 Hz wobble, 5-95% range
  rank.effects.push_back(pwm);

  return rank;
}

Sound::Rank Sound::Rank::ambientDrone(float frequency, int length,
                                      int sampleRate) {
  Rank rank;

  // Detuned base layer (triangles for warmth)
  float detune_cents[] = {-7.0f, 0.0f, 7.0f};
  for (float cents : detune_cents) {
    float detuned_freq = frequency * powf(2.0f, cents / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 0.3f / 3;
    Pipe pipe(note, Sound::WaveForm::Triangular);
    rank.addPipe(pipe);
  }

  // High sine harmonic for shimmer
  Note highNote(frequency * 12.0f, length, sampleRate);
  highNote.volume = 0.005f;
  Pipe highPipe(highNote, Sound::WaveForm::Sine);
  rank.addPipe(highPipe);

  // Slow vibrato for drift
  Effect slowVib;
  slowVib.effectType = Effect::Type::Vibrato;
  slowVib.vibratoConfig = {0.015f, 1.5f}; // 1.5% depth, 1.5 Hz
  rank.effects.push_back(slowVib);

  // Faster vibrato for swirl
  Effect fastVib;
  fastVib.effectType = Effect::Type::Vibrato;
  fastVib.vibratoConfig = {0.02f, 4.0f}; // 2% depth, 4 Hz
  rank.effects.push_back(fastVib);

  return rank;
}

Sound::Rank Sound::Rank::synthStab(float frequency, int length,
                                   int sampleRate) {
  Rank rank;

  // Short, detuned saw (main stab)
  float detune_cents[] = {-3.0f, 0.0f, 3.0f};
  for (float cents : detune_cents) {
    float detuned_freq = frequency * powf(2.0f, cents / 1200.0f);
    Note sawNote(detuned_freq, length / 4, sampleRate);
    sawNote.volume = 0.3f / 3;
    Pipe sawPipe(sawNote, Sound::WaveForm::Saw);
    rank.addPipe(sawPipe);
  }

  // Punchy square with PWM
  Note squareNote(frequency * 1.01f, length / 4, sampleRate);
  squareNote.volume = 0.35f;
  Pipe squarePipe(squareNote, Sound::WaveForm::Square);
  rank.addPipe(squarePipe);

  // Duty cycle for pulse
  Effect pwm;
  pwm.effectType = Effect::Type::DutyCycle;
  pwm.dutyCycleConfig =
      Effect::DutyCycleConfig(8.0f, 0.3f); // 8 Hz pulse, 20-80% range
  rank.effects.push_back(pwm);

  return rank;
}

Sound::Rank Sound::Rank::glassBells(float frequency, int length,
                                    int sampleRate) {
  Rank rank;

  // Base layer: detuned sines and triangles
  float detune_cents[] = {-4.0f, 0.0f, 4.0f};
  for (int i = 0; i < 3; ++i) {
    float detuned_freq = frequency * powf(2.0f, detune_cents[i] / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 0.25f / 3;
    Pipe pipe(note, (i % 2 == 0) ? Sound::WaveForm::Sine
                                 : Sound::WaveForm::Triangular);
    rank.addPipe(pipe);
  }

  // High harmonic for sparkle
  Note highNote(frequency * 3.0f, length, sampleRate);
  highNote.volume = 0.15f;
  Pipe highPipe(highNote, Sound::WaveForm::Sine);
  rank.addPipe(highPipe);

  // Subtle vibrato for choral effect
  Effect vibrato;
  vibrato.effectType = Effect::Type::Vibrato;
  vibrato.vibratoConfig = {0.01f, 2.0f}; // 1% depth, 2 Hz
  rank.effects.push_back(vibrato);

  return rank;
}
