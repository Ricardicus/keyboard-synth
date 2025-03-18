#ifndef KEYBOARD_EFFECTS_HPP
#define KEYBOARD_EFFECTS_HPP

#include <map>
#include <vector>

#include "fir.hpp"
#include "iir.hpp"

class Effect {
public:
  Effect() {}
  Effect(std::vector<FIR> &firs_) { this->firs = firs_; }

  void addFIR(FIR &fir) { this->firs.push_back(fir); }
  std::vector<short> apply(const std::vector<short> &buffer);
  std::vector<short> apply(const std::vector<short> &buffer, size_t maxLen);

  std::vector<short> apply_chorus(const std::vector<short> &buffer);
  std::vector<short> apply_fir(const std::vector<short> &buffer);
  std::vector<short> apply_fir(const std::vector<short> &buffer, size_t maxLen);
  std::vector<short> apply_iir(const std::vector<short> &buffer);
  std::vector<short> apply_iir(const std::vector<short> &buffer, size_t maxLen);

  std::vector<FIR> firs;
  std::vector<IIR> iirs;

  enum Type { Fir, Iir, Chorus, Vibrato };

  typedef struct ChorusConfig {
    float delay;
    float depth;
    int numVoices;
    ChorusConfig(float lfoRate, float depth, int numVoices)
        : delay(lfoRate), depth(depth), numVoices(numVoices) {}
  } ChorusConfig;

  typedef struct VibratoConfig {
    float frequency;
    float depth;
  } VibratoConfig;

  Type effectType = Type::Fir;
  ChorusConfig chorusConfig{0.05f, 3, 3};
  VibratoConfig vibratoConfig{6, 0.3};
  int sampleRate = 44100;

private:
  using Complex = std::complex<double>;
};

#endif
