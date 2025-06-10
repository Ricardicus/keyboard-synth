#ifndef KEYBOARD_EFFECTS_HPP
#define KEYBOARD_EFFECTS_HPP

#include <json.hpp>
#include <map>
#include <optional>
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

  static std::string typeToStr(Type t) {
    switch (t) {
    case Fir:
      return "Fir";
    case Iir:
      return "Iir";
    case Chorus:
      return "Chorus";
    case Vibrato:
      return "Vibrato";
    case DutyCycle:
      return "DutyCycle";
    case Tremolo:
      return "Tremolo";
    }
    return "Unknown (interesting)";
  };

  static nlohmann::json typeToJson(Type t) {
    int tInt = t;
    std::string tStr = typeToStr(t);

    return {
        {"type", tInt},
        {"typeStr", tStr},
    };
  };

  typedef struct ChorusConfig {
    float delay;
    float depth;
    int numVoices;
    ChorusConfig(float lfoRate, float depth, int numVoices)
        : delay(lfoRate), depth(depth), numVoices(numVoices) {}
    nlohmann::json toJson() const {
      return {
          {"delay", this->delay},
          {"depth", this->depth},
          {"numVoices", this->numVoices},
      };
    };
    static std::optional<ChorusConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("delay") || !j["delay"].is_number() ||
          !j.contains("depth") || !j["depth"].is_number() ||
          !j.contains("numVoices") || !j["numVoices"].is_number_integer())
        return std::nullopt;

      return ChorusConfig{j["delay"].get<float>(), j["depth"].get<float>(),
                          j["numVoices"].get<int>()};
    }

  } ChorusConfig;

  typedef struct VibratoConfig {
    float frequency;
    float depth;

    nlohmann::json toJson() const {
      return {
          {"frequency", this->frequency},
          {"depth", this->depth},
      };
    };
    static std::optional<VibratoConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("frequency") || !j["frequency"].is_number() ||
          !j.contains("depth") || !j["depth"].is_number())
        return std::nullopt;

      VibratoConfig v;
      v.frequency = j["frequency"].get<float>();
      v.depth = j["depth"].get<float>();
      return v;
    }

  } VibratoConfig;

  typedef struct DutyCycleConfig {
    float frequency; // Frequency of PWM modulation
    float depth;     // Depth of modulation
    DutyCycleConfig(float frequency, float depth)
        : frequency(frequency), depth(depth) {}
    nlohmann::json toJson() const {
      return {
          {"frequency", this->frequency},
          {"depth", this->depth},
      };
    };

    static std::optional<DutyCycleConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("frequency") || !j["frequency"].is_number() ||
          !j.contains("depth") || !j["depth"].is_number())
        return std::nullopt;

      return DutyCycleConfig{j["frequency"].get<float>(),
                             j["depth"].get<float>()};
    }
  } DutyCycleConfig;

  typedef struct TremoloConfig {
    float frequency;
    float depth;
    nlohmann::json toJson() const {
      return {
          {"frequency", this->frequency},
          {"depth", this->depth},
      };
    };
    static std::optional<TremoloConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("frequency") || !j["frequency"].is_number() ||
          !j.contains("depth") || !j["depth"].is_number())
        return std::nullopt;

      TremoloConfig t;
      t.frequency = j["frequency"].get<float>();
      t.depth = j["depth"].get<float>();
      return t;
    }

  } TremoloConfig;

  Type effectType = Type::Fir;
  ChorusConfig chorusConfig{0.05f, 3, 3};
  VibratoConfig vibratoConfig{6, 0.3};
  DutyCycleConfig dutyCycleConfig{2.0f, 0.5f}; // Default values (2 Hz, 50%)
  TremoloConfig tremoloConfig{18.0f, 1.0f};    // Default values (2 Hz, 50%)
  int sampleRate = 44100;

  nlohmann::json toJson() const {
    std::vector<nlohmann::json> firsJson;
    std::vector<nlohmann::json> iirsJson;
    std::vector<nlohmann::json> iirsfJson;

    for (int i = 0; i < this->firs.size(); i++) {
      firsJson.push_back(this->firs[i].toJson());
    }
    for (int i = 0; i < this->iirs.size(); i++) {
      iirsJson.push_back(this->iirs[i].toJson());
    }
    for (int i = 0; i < this->iirsf.size(); i++) {
      iirsfJson.push_back(this->iirsf[i].toJson());
    }

    return {{"type", typeToJson(this->effectType)},
            {"chorus", this->chorusConfig.toJson()},
            {"vibrato", this->vibratoConfig.toJson()},
            {"dutycycle", this->dutyCycleConfig.toJson()},
            {"tremoloConfig", this->tremoloConfig.toJson()},
            {"firs", firsJson},
            {"iirs", iirsJson},
            {"iirsf", iirsfJson}};
  };

  static std::optional<Effect> fromJson(const nlohmann::json &j) {
    // ---------- validate presence & shape ----------
    if (!j.contains("type") || !j["type"].is_object())
      return std::nullopt;
    if (!j["type"].contains("type") || !j["type"]["type"].is_number_integer())
      return std::nullopt;

    // ---------- parse nested configs ----------
    auto chorusOpt =
        ChorusConfig::fromJson(j.value("chorus", nlohmann::json{}));
    auto vibratoOpt =
        VibratoConfig::fromJson(j.value("vibrato", nlohmann::json{}));
    auto dutyOpt =
        DutyCycleConfig::fromJson(j.value("dutycycle", nlohmann::json{}));
    auto tremOpt =
        TremoloConfig::fromJson(j.value("tremoloConfig", nlohmann::json{}));

    if (!chorusOpt || !vibratoOpt || !dutyOpt || !tremOpt)
      return std::nullopt;

    // ---------- build Effect ----------
    Effect cfg;
    cfg.effectType = static_cast<Type>(j["type"]["type"].get<int>());
    cfg.chorusConfig = *chorusOpt;
    cfg.vibratoConfig = *vibratoOpt;
    cfg.dutyCycleConfig = *dutyOpt;
    cfg.tremoloConfig = *tremOpt;

    // Optional: sampleRate
    if (j.contains("sampleRate") && j["sampleRate"].is_number_integer())
      cfg.sampleRate = j["sampleRate"].get<int>();

    // ---------- parse firs ----------
    if (j.contains("firs") && j["firs"].is_array()) {
      for (const auto &item : j["firs"]) {
        auto firOpt = FIR::fromJson(item);
        if (firOpt) {
          cfg.firs.push_back(*firOpt);
        }
      }
    }

    // ---------- parse iirs (assumed IIR<short>) ----------
    if (j.contains("iirs") && j["iirs"].is_array()) {
      for (const auto &item : j["iirs"]) {
        auto iirOpt = IIR<short>::fromJson(item);
        if (iirOpt) {
          cfg.iirs.push_back(*iirOpt);
        }
      }
    }

    // ---------- parse iirsf (assumed IIR<float>) ----------
    if (j.contains("iirsf") && j["iirsf"].is_array()) {
      for (const auto &item : j["iirsf"]) {
        auto iirfOpt = IIR<float>::fromJson(item);
        if (iirfOpt) {
          cfg.iirsf.push_back(*iirfOpt);
        }
      }
    }

    return cfg;
  }

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

  nlohmann::json toJson() const {
    return {
        {"feedback", this->feedback},
        {"mix", this->mix},
        {"sampleRate", this->sampleRate},
    };
  };

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
