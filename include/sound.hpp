#ifndef KEYBOARD_SOUND_HPP
#define KEYBOARD_SOUND_HPP

#include "adsr.hpp"
#include "effect.hpp"
#include "note.hpp"
#include <vector>
namespace Sound {
enum WaveForm { Sine, Triangular, Square, Saw, WaveFile };

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
  ADSR adsr;
  std::vector<Pipe> pipes;
  std::vector<Effect> effects;
  enum Preset { SuperSaw, None };
  static std::string presetStr(Preset p) {
    std::string result;
    switch (p) {
    case SuperSaw:
      result = "SuperSaw";
      break;
    case None:
      result = "None";
      break;
    }
    return result;
  }
};

std::vector<short> generateWave(Rank &rank);

} // namespace Sound

#endif
