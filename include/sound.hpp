#ifndef KEYBOARD_SOUND_HPP
#define KEYBOARD_SOUND_HPP

#include "adsr.hpp"
#include "effect.hpp"
#include "note.hpp"
#include <vector>
namespace Sound {
enum WaveForm { Sine, Triangular, Square, Saw, WaveFile };

float sinus(float f);
float square(float f);
float square(float f, float factor);
float triangular(float f);
float saw(float f);
float white_noise(float f);

std::string typeOfWave(WaveForm form);
std::vector<short> generateWave(WaveForm f, Note &note, ADSR &adsr,
                                std::vector<Effect> &effects);
std::vector<short> generateSineWave(Note &note, ADSR &adsr,
                                    std::vector<Effect> &effects);
std::vector<short> generateSquareWave(Note &note, ADSR &adsr,
                                      std::vector<Effect> &effects);
std::vector<short> generateSawWave(Note &note, ADSR &adsr,
                                   std::vector<Effect> &effects);
std::vector<short> generateTriangularWave(Note &note, ADSR &adsr,
                                          std::vector<Effect> &effects);

using Pipe = std::pair<Note, Sound::WaveForm>;

class Rank {
public:
  Rank() {}
  Rank(ADSR &adsr) { this->adsr = adsr; }
  void addPipe(Pipe &pipe) { this->pipes.push_back(pipe); }
  void addEffect(Effect &effect) { this->effects.push_back(effect); }

  static Rank superSaw(float frequency, int length, int sampleRate);
  static Rank fatTriangle(float frequency, int length, int sampleRate);
  static Rank pulseSquare(float frequency, int length, int sampleRate);
  static Rank sineSawDrone(float frequency, int length, int sampleRate);
  static Rank superSawWithSub(float frequency, int length, int sampleRate);
  static Rank glitchMix(float frequency, int length, int sampleRate);
  static Rank lushPad(float frequency, int length, int sampleRate);
  static Rank retroLead(float frequency, int length, int sampleRate);
  static Rank organTone(float frequency, int length, int sampleRate);
  static Rank bassGrowl(float frequency, int length, int sampleRate);
  static Rank ambientDrone(float frequency, int length, int sampleRate);
  static Rank synthStab(float frequency, int length, int sampleRate);
  static Rank glassBells(float frequency, int length, int sampleRate);
  static Rank sine(float frequency, int length, int sampleRate);
  static Rank saw(float frequency, int length, int sampleRate);
  static Rank square(float frequency, int length, int sampleRate);
  static Rank triangular(float frequency, int length, int sampleRate);

  ADSR adsr;
  std::vector<Pipe> pipes;
  std::vector<Effect> effects;
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
    GlassBells,
    Sine,
    Triangular,
    Square,
    Saw,
    None
  };

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

  float generateRankSample();
  float generateRankSampleIndex(int index);

  static Sound::Rank::Preset fromString(const std::string &str_) {
    std::string str = str_;
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    Sound::Rank::Preset result;
    if (str == "triangular") {
      result = Sound::Rank::Preset::Triangular;
    } else if (str == "saw") {
      result = Sound::Rank::Preset::Saw;
    } else if (str == "square") {
      result = Sound::Rank::Preset::Square;
    } else if (str == "triangular") {
      result = Sound::Rank::Preset::Triangular;
    } else if (str == "sine") {
      result = Sound::Rank::Preset::Sine;
    } else if (str == "supersaw") {
      result = Sound::Rank::Preset::SuperSaw;
    } else if (str == "fattriangle") {
      result = Sound::Rank::Preset::FatTriangle;
    } else if (str == "pulsesquare") {
      result = Sound::Rank::Preset::PulseSquare;
    } else if (str == "sinesawdrone") {
      result = Sound::Rank::Preset::SineSawDrone;
    } else if (str == "supersawsub") {
      result = Sound::Rank::Preset::SuperSawWithSub;
    } else if (str == "glitchmix") {
      result = Sound::Rank::Preset::GlitchMix;
    } else if (str == "lushpad") {
      result = Sound::Rank::Preset::LushPad;
    } else if (str == "retrolead") {
      result = Sound::Rank::Preset::RetroLead;
    } else if (str == "bassgrowl") {
      result = Sound::Rank::Preset::BassGrowl;
    } else if (str == "ambientdrone") {
      result = Sound::Rank::Preset::AmbientDrone;
    } else if (str == "synthstab") {
      result = Sound::Rank::Preset::SynthStab;
    } else if (str == "glassbells") {
      result = Sound::Rank::Preset::GlassBells;
    } else if (str == "organtone") {
      result = Sound::Rank::Preset::OrganTone;
    }
    return result;
  }

  static Rank fromPreset(Sound::Rank::Preset preset, float frequency,
                         int length, int sampleRate) {
    Sound::Rank r;
    switch (preset) {
    case Sound::Rank::Preset::SuperSaw:
      r = Sound::Rank::superSaw(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::FatTriangle:
      r = Sound::Rank::fatTriangle(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::PulseSquare:
      r = Sound::Rank::pulseSquare(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::SineSawDrone:
      r = Sound::Rank::sineSawDrone(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::SuperSawWithSub:
      r = Sound::Rank::superSawWithSub(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::GlitchMix:
      r = Sound::Rank::glitchMix(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::LushPad:
      r = Sound::Rank::lushPad(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::RetroLead:
      r = Sound::Rank::retroLead(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::BassGrowl:
      r = Sound::Rank::bassGrowl(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::AmbientDrone:
      r = Sound::Rank::ambientDrone(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::SynthStab:
      r = Sound::Rank::synthStab(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::GlassBells:
      r = Sound::Rank::glassBells(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::OrganTone:
      r = Sound::Rank::organTone(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::Saw:
      r = Sound::Rank::saw(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::Square:
      r = Sound::Rank::square(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::Triangular:
      r = Sound::Rank::triangular(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::Sine:
      r = Sound::Rank::sine(frequency, length, sampleRate);
      break;
    case Sound::Rank::Preset::None:
      break;
    }
    return r;
  }

  void reset() { this->generatorIndex = 0; }

private:
  unsigned int generatorIndex = 0;
};

std::vector<short> generateWave(Rank &rank);

} // namespace Sound

#endif
