#ifndef KEYBOARD_SOUND_HPP
#define KEYBOARD_SOUND_HPP

#include "adsr.hpp"
#include "effect.hpp"
#include "note.hpp"
#include <json.hpp>
#include <vector>

namespace Sound {
enum WaveForm { Sine, Triangular, Square, Saw, WhiteNoise, WaveFile };

float sinus(float f);
float square(float f);
float square(float f, float factor);
float triangular(float f);
float saw(float f);
float white_noise(float f);

std::string typeOfWave(WaveForm form);
std::vector<short> generateWave(WaveForm f, Note &note, ADSR &adsr,
                                std::vector<Effect<short>> &effects);
std::vector<short> generateSineWave(Note &note, ADSR &adsr,
                                    std::vector<Effect<short>> &effects);
std::vector<short> generateSquareWave(Note &note, ADSR &adsr,
                                      std::vector<Effect<short>> &effects);
std::vector<short> generateSawWave(Note &note, ADSR &adsr,
                                   std::vector<Effect<short>> &effects);
std::vector<short> generateWhiteNoiseWave(Note &note, ADSR &adsr,
                                          std::vector<Effect<short>> &effects);
std::vector<short> generateTriangularWave(Note &note, ADSR &adsr,
                                          std::vector<Effect<short>> &effects);

using Pipe = std::pair<Note, Sound::WaveForm>;

template <typename T> class Rank {
public:
  Rank() {}
  Rank(ADSR &adsr) { this->adsr = adsr; }
  void addPipe(Pipe &pipe) { this->pipes.push_back(pipe); }
  void addEffect(Effect<T> &effect) { this->effects.push_back(effect); }

  ADSR adsr;
  std::vector<Pipe> pipes;
  std::vector<Effect<T>> effects;
  enum Preset {
    SuperSaw,
    FatTriangle,
    PulseSquare,
    SineSawDrone,
    SuperSawWithSub,
    GlitchMix,
    OrganTone,
    LushPad,
    RetroLead,
    BassGrowl,
    AmbientDrone,
    SynthStab,
    FluteBreathy,
    GlassBells,
    Sine,
    Triangular,
    Square,
    Saw,
    None
  };

  static nlohmann::json presetToJson(Preset p) {
    int pInt = p;
    std::string pStr = presetStr(p);
    return {{"type", pInt}, {"name", pStr}};
  };

  static std::optional<Preset> jsonToPreset(const nlohmann::json &j) {
    if (!j.is_object() || !j.contains("type") || !j["type"].is_number_integer())
      return std::nullopt;

    int type = j["type"].get<int>();
    if (type < static_cast<int>(Preset::SuperSaw) ||
        type > static_cast<int>(Preset::None))
      return std::nullopt;

    return static_cast<Preset>(type);
  }

  static std::string presetStr(Preset p) {
    std::string result;
    switch (p) {
    case SuperSaw:
      result = "SuperSaw";
      break;
    case FatTriangle:
      result = "FatTriangle";
      break;
    case PulseSquare:
      result = "PulseSquare";
      break;
    case SineSawDrone:
      result = "SineSawDrone";
      break;
    case SuperSawWithSub:
      result = "SuperSawWithSub";
      break;
    case GlitchMix:
      result = "GlitchMix";
      break;
    case LushPad:
      result = "LushPad";
      break;
    case RetroLead:
      result = "RetroLead";
      break;
    case BassGrowl:
      result = "BassGrowl";
      break;
    case AmbientDrone:
      result = "AmbientDrone";
      break;
    case SynthStab:
      result = "SynthStab";
      break;
    case FluteBreathy:
      result = "FluteBreathy";
      break;
    case GlassBells:
      result = "GlassBells";
      break;
    case OrganTone:
      result = "OrganTone";
      break;
    case Sine:
      result = "Sine";
      break;
    case Triangular:
      result = "Triangular";
      break;
    case Saw:
      result = "Saw";
      break;
    case Square:
      result = "Square";
      break;
    case None:
      result = "None";
      break;
    }
    return result;
  }

  T generateRankSample();
  T generateRankSampleIndex(int index);

  static Sound::Rank<T>::Preset fromString(const std::string &str_) {
    std::string str = str_;
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    Sound::Rank<T>::Preset result;
    if (str == "triangular") {
      result = Sound::Rank<T>::Preset::Triangular;
    } else if (str == "saw") {
      result = Sound::Rank<T>::Preset::Saw;
    } else if (str == "square") {
      result = Sound::Rank<T>::Preset::Square;
    } else if (str == "triangular") {
      result = Sound::Rank<T>::Preset::Triangular;
    } else if (str == "sine") {
      result = Sound::Rank<T>::Preset::Sine;
    } else if (str == "supersaw") {
      result = Sound::Rank<T>::Preset::SuperSaw;
    } else if (str == "fattriangle") {
      result = Sound::Rank<T>::Preset::FatTriangle;
    } else if (str == "pulsesquare") {
      result = Sound::Rank<T>::Preset::PulseSquare;
    } else if (str == "sinesawdrone") {
      result = Sound::Rank<T>::Preset::SineSawDrone;
    } else if (str == "supersawsub") {
      result = Sound::Rank<T>::Preset::SuperSawWithSub;
    } else if (str == "glitchmix") {
      result = Sound::Rank<T>::Preset::GlitchMix;
    } else if (str == "lushpad") {
      result = Sound::Rank<T>::Preset::LushPad;
    } else if (str == "retrolead") {
      result = Sound::Rank<T>::Preset::RetroLead;
    } else if (str == "bassgrowl") {
      result = Sound::Rank<T>::Preset::BassGrowl;
    } else if (str == "ambientdrone") {
      result = Sound::Rank<T>::Preset::AmbientDrone;
    } else if (str == "synthstab") {
      result = Sound::Rank<T>::Preset::SynthStab;
    } else if (str == "flutebreath") {
      result = Sound::Rank<T>::Preset::FluteBreathy;
    } else if (str == "glassbells") {
      result = Sound::Rank<T>::Preset::GlassBells;
    } else if (str == "organtone") {
      result = Sound::Rank<T>::Preset::OrganTone;
    }
    return result;
  }

  static Rank<T> fromPreset(Sound::Rank<T>::Preset preset, float frequency,
                            int length, int sampleRate) {
    static Sound::Rank<T> r;
    switch (preset) {
    case Sound::Rank<T>::Preset::SuperSaw:
      r = Sound::Rank<T>::superSaw(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::FatTriangle:
      r = Sound::Rank<T>::fatTriangle(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::PulseSquare:
      r = Sound::Rank<T>::pulseSquare(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::SineSawDrone:
      r = Sound::Rank<T>::sineSawDrone(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::SuperSawWithSub:
      r = Sound::Rank<T>::superSawWithSub(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::GlitchMix:
      r = Sound::Rank<T>::glitchMix(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::LushPad:
      r = Sound::Rank<T>::lushPad(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::RetroLead:
      r = Sound::Rank<T>::retroLead(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::BassGrowl:
      r = Sound::Rank<T>::bassGrowl(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::AmbientDrone:
      r = Sound::Rank<T>::ambientDrone(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::SynthStab:
      r = Sound::Rank<T>::synthStab(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::GlassBells:
      r = Sound::Rank<T>::glassBells(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::OrganTone:
      r = Sound::Rank<T>::organTone(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::FluteBreathy:
      r = Sound::Rank<T>::fluteBreathy(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::Saw:
      r = Sound::Rank<T>::saw(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::Square:
      r = Sound::Rank<T>::square(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::Triangular:
      r = Sound::Rank<T>::triangular(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::Sine:
      r = Sound::Rank<T>::sine(frequency, length, sampleRate);
      break;
    case Sound::Rank<T>::Preset::None:
      break;
    }
    return r;
  }

  void reset() { this->generatorIndex = 0; }

  static Sound::Rank<T> superSaw(float frequency, int length, int sampleRate) {
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

  static Sound::Rank<T> fatTriangle(float frequency, int length,
                                    int sampleRate) {
    Rank rank;
    float detune = 0.3f; // Subtle detuning
    float detune_cents[] = {-10.0f, -5.0f, 0.0f, 5.0f, 10.0f};
    const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

    for (int i = 0; i < num_oscillators; ++i) {
      float detuned_freq =
          frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
      Note note(detuned_freq, length, sampleRate);
      note.volume =
          0.8f / num_oscillators; // Slightly lower amplitude for warmth
      Pipe pipe(note, Sound::WaveForm::Triangular);
      rank.addPipe(pipe);
    }

    // Optional: Add vibrato for extra depth
    Effect<T> effect;
    effect.effectType = Effect<T>::Type::Vibrato;
    effect.config = typename Effect<T>::VibratoConfig{4.0f, 0.015f};
    rank.effects.push_back(effect); // 1.5% depth, 4 Hz

    return rank;
  }

  static Sound::Rank<T> pulseSquare(float frequency, int length,
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
    Effect<T> effect;
    effect.effectType = Effect<T>::Type::Vibrato;
    effect.config = typename Effect<T>::VibratoConfig{6.0f, 0.03f};
    rank.effects.push_back(effect);

    Effect<T> pwmEffect;
    pwmEffect.effectType = Effect<T>::Type::DutyCycle;
    pwmEffect.config = typename Effect<T>::DutyCycleConfig{3.0f, 0.9f};
    rank.effects.push_back(pwmEffect);

    return rank;
  }

  static Sound::Rank<T> sineSawDrone(float frequency, int length,
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
    Effect<T> effect;
    effect.effectType = Effect<T>::Type::Vibrato;
    effect.config = typename Effect<T>::VibratoConfig{2.5f, 0.01f};
    rank.effects.push_back(effect);

    return rank;
  }

  static Sound::Rank<T> superSawWithSub(float frequency, int length,
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

  static Sound::Rank<T> glitchMix(float frequency, int length, int sampleRate) {
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
    Effect<T> effect;
    effect.effectType = Effect<T>::Type::Vibrato;
    effect.config = typename Effect<T>::VibratoConfig{0.05f, 10.0f};
    rank.effects.push_back(effect);

    return rank;
  }

  static Sound::Rank<T> lushPad(float frequency, int length, int sampleRate) {
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

    Effect<T> chorusEffect;
    chorusEffect.effectType = Effect<T>::Type::Chorus;
    chorusEffect.config = typename Effect<T>::ChorusConfig{0.03f, 0.2f, 3};
    rank.effects.push_back(chorusEffect);

    return rank;
  }

  static Sound::Rank<T> retroLead(float frequency, int length, int sampleRate) {
    Rank rank;

    Note squareNote(frequency, length, sampleRate);
    squareNote.volume = 0.8f;
    Pipe squarePipe(squareNote, Sound::WaveForm::Square);
    rank.addPipe(squarePipe);

    Effect<T> pwmEffect;
    pwmEffect.effectType = Effect<T>::Type::DutyCycle;
    pwmEffect.config = typename Effect<T>::DutyCycleConfig{5.0f, 0.4f};
    rank.effects.push_back(pwmEffect);

    return rank;
  }

  static Sound::Rank<T> organTone(float frequency, int length, int sampleRate) {
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
    Effect<T> tremolo;
    tremolo.effectType = Effect<T>::Type::Vibrato;
    tremolo.config = typename Effect<T>::VibratoConfig{
        0.05f, 7.0f}; // 5% depth, 7 Hz for spinning feel
    rank.effects.push_back(tremolo);

    // Add a subtle saw for grit
    Note sawNote(frequency * 1.01f, length, sampleRate); // Slight detune
    sawNote.volume = 0.03f;
    Pipe sawPipe(sawNote, Sound::WaveForm::Saw);
    rank.addPipe(sawPipe);

    return rank;
  }

  static Sound::Rank<T> bassGrowl(float frequency, int length, int sampleRate) {
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
    Effect<T> pwm;
    pwm.effectType = Effect<T>::Type::DutyCycle;
    pwm.config = typename Effect<T>::DutyCycleConfig{
        3.0f, 0.45f}; // 3 Hz wobble, 5-95% range
    rank.effects.push_back(pwm);

    return rank;
  }

  static Sound::Rank<T> ambientDrone(float frequency, int length,
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
    Effect<T> slowVib;
    slowVib.effectType = Effect<T>::Type::Vibrato;
    slowVib.config =
        typename Effect<T>::VibratoConfig{0.015f, 1.5f}; // 1.5% depth, 1.5 Hz
    rank.effects.push_back(slowVib);

    // Faster vibrato for swirl
    Effect<T> fastVib;
    fastVib.effectType = Effect<T>::Type::Vibrato;
    fastVib.config =
        typename Effect<T>::VibratoConfig{0.02f, 4.0f}; // 2% depth, 4 Hz
    rank.effects.push_back(fastVib);

    return rank;
  }

  static Sound::Rank<T> synthStab(float frequency, int length, int sampleRate) {
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
    Effect<T> pwm;
    pwm.effectType = Effect<T>::Type::DutyCycle;
    pwm.config = typename Effect<T>::DutyCycleConfig{
        8.0f, 0.3f}; // 8 Hz pulse, 20-80% range
    rank.effects.push_back(pwm);

    return rank;
  }

  static Sound::Rank<T> glassBells(float frequency, int length,
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
    Effect<T> vibrato;
    vibrato.effectType = Effect<T>::Type::Vibrato;
    vibrato.config =
        typename Effect<T>::VibratoConfig{0.01f, 2.0f}; // 1% depth, 2 Hz
    rank.effects.push_back(vibrato);

    return rank;
  }

  static Sound::Rank<T> sine(float frequency, int length, int sampleRate) {
    Rank rank;

    Note sineNote(frequency, length, sampleRate);
    Pipe sinePipe(sineNote, Sound::WaveForm::Sine);
    rank.addPipe(sinePipe);

    return rank;
  }

  static Sound::Rank<T> saw(float frequency, int length, int sampleRate) {
    Rank rank;

    Note note(frequency, length, sampleRate);
    Pipe pipe(note, Sound::WaveForm::Saw);
    rank.addPipe(pipe);

    return rank;
  }

  static Sound::Rank<T> square(float frequency, int length, int sampleRate) {
    Rank rank;

    Note note(frequency, length, sampleRate);
    Pipe pipe(note, Sound::WaveForm::Square);
    rank.addPipe(pipe);

    return rank;
  }

  static Sound::Rank<T> triangular(float frequency, int length,
                                   int sampleRate) {
    Rank rank;

    Note note(frequency, length, sampleRate);
    Pipe pipe(note, Sound::WaveForm::Triangular);
    rank.addPipe(pipe);

    return rank;
  }

  static Sound::Rank<T> fluteBreathy(float f, int length, int sr) {
    Rank rank;

    // Core tone: mostly sine, tiny harmonics
    Note f0(f, length, sr);
    f0.volume = 0.85f;
    Pipe p0(f0, Sound::WaveForm::Sine);
    rank.addPipe(p0);
    Note h2(2.0f * f, length, sr);
    h2.volume = 0.10f;
    Pipe p2(h2, Sound::WaveForm::Sine);
    rank.addPipe(p2);
    Note h3(3.0f * f, length, sr);
    h3.volume = 0.05f;
    Pipe p3(h3, Sound::WaveForm::Sine);
    rank.addPipe(p3);

    // Breath noise: short, quiet
    Note breath(f, length / 6, sr); // duration just for a little "chiff"
    breath.volume = 0.03f;
    Pipe p4(breath, Sound::WaveForm::WhiteNoise);
    rank.addPipe(p4);

    // Gentle vibrato (depth, rateHz)
    Effect<T> vib;
    vib.effectType = Effect<T>::Type::Vibrato;
    vib.config = typename Effect<T>::VibratoConfig{0.02f, 5.5f};
    rank.effects.push_back(vib);

    return rank;
  }

private:
  unsigned int generatorIndex = 0;
};

std::vector<short> generateWave(Rank<short> &rank);

template <typename T>
T applyPostEffects(T sample, std::vector<Effect<T>> &effects);

} // namespace Sound

#endif
