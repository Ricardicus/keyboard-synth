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
  std::vector<IIR<short>> iirs;
  std::vector<IIR<float>> iirsf;

  enum Type { Fir, Iir, Chorus, Vibrato, DutyCycle, Tremolo };

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

  typedef struct DutyCycleConfig {
    float frequency; // Frequency of PWM modulation
    float depth;     // Depth of modulation
    DutyCycleConfig(float frequency, float depth)
        : frequency(frequency), depth(depth) {}
  } DutyCycleConfig;

  typedef struct TremoloConfig {
    float frequency;
    float depth;
  } TremoloConfig;

  Type effectType = Type::Fir;
  ChorusConfig chorusConfig{0.05f, 3, 3};
  VibratoConfig vibratoConfig{6, 0.3};
  DutyCycleConfig dutyCycleConfig{2.0f, 0.5f}; // Default values (2 Hz, 50%)
  TremoloConfig tremoloConfig{18.0f, 1.0f};    // Default values (2 Hz, 50%)
  int sampleRate = 44100;

private:
  using Complex = std::complex<double>;
};

class EchoEffect {
public:
  EchoEffect(float rateSeconds, float feedback, float mix, float sampleRate);

  void setRate(float rateSeconds);
  void setFeedback(float feedback);
  void setMix(float mix);
  void setSampleRate(float sampleRate);

  float getRate() const {
    return static_cast<float>(delaySamples) / sampleRate;
  }
  float getFeedback() const { return feedback; }
  float getMix() const { return mix; }
  float getSampleRate() const { return sampleRate; }

  float process(float inputSample);

private:
  std::vector<float> buffer;
  size_t writeIndex;
  size_t delaySamples;
  float feedback;
  float mix;
  float sampleRate;

  void updateBuffer();
};

#endif
