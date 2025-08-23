#ifndef KEYBOARD_EFFECTS_HPP
#define KEYBOARD_EFFECTS_HPP

#include <cmath>
#include <json.hpp>
#include <map>
#include <optional>
#include <variant>
#include <vector>

#include "fir.hpp"
#include "iir.hpp"

#ifndef SAMPLERATE
#define SAMPLERATE 44100
#endif

template <typename T> class Adder;
template <typename T> class Piper;

template <typename T> class AllPassEffect {
public:
  AllPassEffect(size_t delaySamples, T g) : buf(delaySamples, 0), g(g), w(0) {}
  T process(T x) {
    size_t r = (w + 1) % buf.size();
    T y = -g * x + buf[r] + g * z; // z = earlier output
    buf[w] = x + g * y;
    z = y;
    w = r;
    return y;
  }

  // ── JSON serialisation ────────────────────────────────────────────────
  nlohmann::json toJson() const {
    return {{"delaySamples", buf.size()}, {"gain", g}};
  }

  static std::optional<AllPassEffect> fromJson(const nlohmann::json &j) {
    // required keys
    if (!j.contains("delaySamples") || !j.contains("gain"))
      return std::nullopt;

    if (!j["delaySamples"].is_number() || !j["gain"].is_number())
      return std::nullopt;

    size_t size = j["delaySamples"].get<int>();
    float gain = j["gain"].get<float>();

    return AllPassEffect(size, gain);
  }
  std::vector<T> buf;
  T g, z{0};
  size_t w;
};

template <typename T> class EchoEffect {
public:
  EchoEffect(float rateSeconds = 1.0f, float feedback = 0.3f, float mix = 1.0f,
             float sampleRate = static_cast<float>(SAMPLERATE))
      : writeIndex(0), delaySamples(0), feedback(feedback), mix(mix),
        sampleRate(sampleRate) {
    setRate(rateSeconds);
  }

  // ── setters ────────────────────────────────────────────────────────────
  void setFeedback(float fb) { feedback = std::clamp(fb, 0.0f, 1.0f); }
  void setMix(float m) { mix = std::clamp(m, 0.0f, 1.0f); }
  void setSampleRate(float sr) {
    sampleRate = sr;
    setRate(getRate());
  }

  // ── getters ────────────────────────────────────────────────────────────
  float getRate() const {
    return static_cast<float>(delaySamples) / sampleRate;
  }
  float getFeedback() const { return feedback; }
  float getMix() const { return mix; }
  float getSampleRate() const { return sampleRate; }

  T process(T inputSample);

  // ── JSON serialisation ────────────────────────────────────────────────
  nlohmann::json toJson() const {
    return {{"rateSeconds", getRate()},
            {"feedback", feedback},
            {"mix", mix},
            {"sampleRate", sampleRate}};
  }

  static std::optional<EchoEffect> fromJson(const nlohmann::json &j) {
    // required keys
    if (!j.contains("rateSeconds") || !j.contains("feedback") ||
        !j.contains("mix") || !j.contains("sampleRate"))
      return std::nullopt;

    if (!j["rateSeconds"].is_number() || !j["feedback"].is_number() ||
        !j["mix"].is_number() || !j["sampleRate"].is_number())
      return std::nullopt;

    float rateSec = j["rateSeconds"].get<float>();
    float fb = j["feedback"].get<float>();
    float mx = j["mix"].get<float>();
    float sr = j["sampleRate"].get<float>();

    if (rateSec <= 0.0f || sr <= 0.0f || fb < 0.0f || fb > 1.0f || mx < 0.0f ||
        mx > 1.0f)
      return std::nullopt;

    return EchoEffect(rateSec, fb, mx, sr);
  }

  void setRate(float rateSeconds) {
    // compute new size in samples, never zero
    size_t newDelay =
        static_cast<size_t>(std::max(rateSeconds * sampleRate, 1.0f));

    if (newDelay > delaySamples) {
      // grow: keep existing data, zero-fill the new tail
      buffer.resize(newDelay, 0.0f);
    } else if (newDelay < delaySamples) {
      // shrink: just shorten the window, preserve the first newDelay samples
      buffer.resize(newDelay);
      // make sure writeIndex still in range [0, newDelay)
      writeIndex %= newDelay;
    }
    // commit the new length
    delaySamples = newDelay;
  }

  float feedback;
  float mix;
  float sampleRate;

private:
  std::vector<T> buffer;
  size_t writeIndex;
  size_t delaySamples;

  void updateBuffer() {
    buffer.assign(delaySamples > 0 ? delaySamples : 1, 0.0f);
    writeIndex = 0;
  }
};

template <typename T> class Effect {
public:
  // ── nested config structs ─────────────────────────────────────────────
  struct ChorusConfig {
    float delay;   // seconds
    float depth;   // seconds
    int numVoices; // number of LFO voices
    nlohmann::json toJson() const {
      return {{"delay", delay}, {"depth", depth}, {"numVoices", numVoices}};
    }
    static std::optional<ChorusConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("delay") || !j.contains("depth") ||
          !j.contains("numVoices"))
        return std::nullopt;
      if (!j["delay"].is_number() || !j["depth"].is_number() ||
          !j["numVoices"].is_number_integer())
        return std::nullopt;
      return ChorusConfig{j["delay"].get<float>(), j["depth"].get<float>(),
                          j["numVoices"].get<int>()};
    }
  };

  struct VibratoConfig {
    float frequency; // Hz
    float depth;     // semitones / ratio
    nlohmann::json toJson() const {
      return {{"frequency", frequency}, {"depth", depth}};
    }
    static std::optional<VibratoConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("frequency") || !j.contains("depth"))
        return std::nullopt;
      if (!j["frequency"].is_number() || !j["depth"].is_number())
        return std::nullopt;
      VibratoConfig v{j["frequency"].get<float>(), j["depth"].get<float>()};
      return v;
    }
  };

  struct DutyCycleConfig {
    float frequency; // Hz
    float depth;     // duty (0..1)
    nlohmann::json toJson() const {
      return {{"frequency", frequency}, {"depth", depth}};
    }
    static std::optional<DutyCycleConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("frequency") || !j.contains("depth"))
        return std::nullopt;
      if (!j["frequency"].is_number() || !j["depth"].is_number())
        return std::nullopt;
      return DutyCycleConfig{j["frequency"].get<float>(),
                             j["depth"].get<float>()};
    }
  };

  struct TremoloConfig {
    float frequency; // Hz
    float depth;     // 0..1
    nlohmann::json toJson() const {
      return {{"frequency", frequency}, {"depth", depth}};
    }
    static std::optional<TremoloConfig> fromJson(const nlohmann::json &j) {
      if (!j.contains("frequency") || !j.contains("depth"))
        return std::nullopt;
      if (!j["frequency"].is_number() || !j["depth"].is_number())
        return std::nullopt;
      TremoloConfig t{j["frequency"].get<float>(), j["depth"].get<float>()};
      return t;
    }
  };

  struct PhaseDistortionSinConfig {
    float depth; // duty (0..1)
    nlohmann::json toJson() const { return {{"depth", depth}}; }
    static std::optional<PhaseDistortionSinConfig>
    fromJson(const nlohmann::json &j) {
      if (!j.contains("depth"))
        return std::nullopt;
      if (!j["depth"].is_number())
        return std::nullopt;
      return PhaseDistortionSinConfig{j["depth"].get<float>()};
    }
  };

  // ── enum & variant ───────────────────────────────────────────────────
  enum Type {
    Fir,
    Iir,
    Chorus,
    Vibrato,
    DutyCycle,
    PhaseDistortionSin,
    Tremolo,
    Echo,
    AllPass,
    Sum,
    Pipe
  };
  using ConfigVariant =
      std::variant<std::monostate, ChorusConfig, VibratoConfig, DutyCycleConfig,
                   TremoloConfig, EchoEffect<T>, AllPassEffect<T>, Adder<T>,
                   Piper<T>, PhaseDistortionSinConfig>;

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
    case Echo:
      return "Echo";
    case AllPass:
      return "AllPass";
    case Sum:
      return "Adder";
    case Pipe:
      return "Piper";
    default:
      return "Unknown";
    }
  }

  static nlohmann::json typeToJson(Type t) {
    return {{"type", static_cast<int>(t)}, {"typeStr", typeToStr(t)}};
  }

  // ── helpers for std::variant access ──────────────────────────────────
  template <typename B> static const B *getIf(const ConfigVariant &v) {
    return std::get_if<B>(&v);
  }
  template <typename B> static B *getIf(ConfigVariant &v) {
    return std::get_if<B>(&v);
  }

  // ── data members ─────────────────────────────────────────────────────
  Type effectType = Fir;
  ConfigVariant config = std::monostate{};

  std::vector<FIR> firs;
  std::vector<IIR<T>> iirs;
  int sampleRate = SAMPLERATE;

  // ── ctor helpers ─────────────────────────────────────────────────────
  Effect() = default;
  Effect(std::vector<FIR> &firs_) { this->firs = firs_; }

  // ── JSON serialisation ───────────────────────────────────────────────
  nlohmann::json toJson() const {
    nlohmann::json j = {{"type", typeToJson(effectType)}};

    auto add = [&](auto &&tag, auto &&ptr) {
      if (ptr)
        j[tag] = ptr->toJson();
    };
    switch (effectType) {
    case Chorus:
      add("chorus", getIf<ChorusConfig>(config));
      break;
    case Vibrato:
      add("vibrato", getIf<VibratoConfig>(config));
      break;
    case DutyCycle:
      add("dutycycle", getIf<DutyCycleConfig>(config));
      break;
    case Tremolo:
      add("tremolo", getIf<TremoloConfig>(config));
      break;
    case AllPass:
      add("allpass", getIf<AllPassEffect<T>>(config));
    case Echo:
      add("echo", getIf<EchoEffect<T>>(config));
      break;
    default:
      break; // Fir / Iir have no extra JSON fields
    }

    if (!firs.empty())
      for (const auto &f : firs)
        j["firs"].push_back(f.toJson());
    if (!iirs.empty())
      for (const auto &f : iirs)
        j["iirs"].push_back(f.toJson());
    if (sampleRate != SAMPLERATE)
      j["sampleRate"] = sampleRate;
    return j;
  }

  static std::optional<Effect<T>> fromJson(const nlohmann::json &j) {
    // validate effect type object
    if (!j.contains("type") || !j["type"].is_object() ||
        !j["type"].contains("type") || !j["type"]["type"].is_number_integer())
      return std::nullopt;

    Effect<T> e;
    e.effectType = static_cast<Type>(j["type"]["type"].get<int>());

    auto trySetConfig = [&](auto &&tag, auto fromJsonFn) {
      if (!j.contains(tag))
        return true; // optional if absent
      auto opt = fromJsonFn(j[tag]);
      if (!opt)
        return false;
      e.config = *opt;
      return true;
    };

    switch (e.effectType) {
    case Chorus:
      if (!trySetConfig("chorus", ChorusConfig::fromJson))
        return std::nullopt;
      break;
    case Vibrato:
      if (!trySetConfig("vibrato", VibratoConfig::fromJson))
        return std::nullopt;
      break;
    case DutyCycle:
      if (!trySetConfig("dutycycle", DutyCycleConfig::fromJson))
        return std::nullopt;
      break;
    case Tremolo:
      if (!trySetConfig("tremolo", TremoloConfig::fromJson))
        return std::nullopt;
      break;
    case AllPass:
      if (!trySetConfig("allpass", AllPassEffect<T>::fromJson))
        return std::nullopt;
      break;
    case Echo:
      if (!trySetConfig("echo", EchoEffect<T>::fromJson))
        return std::nullopt;
      break;
    default:
      break; // Fir / Iir
    }

    // FIR / IIR arrays
    if (j.contains("firs"))
      for (const auto &node : j["firs"])
        if (auto f = FIR::fromJson(node))
          e.firs.push_back(*f);
    if (j.contains("iirs"))
      for (const auto &node : j["iirs"])
        if (auto f = IIR<T>::fromJson(node))
          e.iirs.push_back(*f);

    if (j.contains("sampleRate") && j["sampleRate"].is_number_integer())
      e.sampleRate = j["sampleRate"].get<int>();

    return e;
  }

  void addFIR(FIR &fir) { this->firs.push_back(fir); }
  std::vector<T> apply(const std::vector<T> &buffer);
  std::vector<T> apply(const std::vector<T> &buffer, size_t maxLen);

  std::vector<T> apply_chorus(const std::vector<T> &buffer);
  std::vector<T> apply_fir(const std::vector<T> &buffer);
  std::vector<T> apply_fir(const std::vector<T> &buffer, size_t maxLen);
  std::vector<T> apply_iir(const std::vector<T> &buffer);
  std::vector<T> apply_iir(const std::vector<T> &buffer, size_t maxLen);
};

template <typename T> class Adder {
public:
  Adder(std::vector<Effect<T>> effects) : effects(effects) {}
  T process(T x) {
    T result = x;
    for (int e = 0; e < effects.size(); e++) {
      if (auto echo = std::get_if<EchoEffect<T>>(&effects[e].config)) {
        result += echo->process(result);
      } else if (auto allpass =
                     std::get_if<AllPassEffect<T>>(&effects[e].config)) {
        result += allpass->process(result);
      } else if (auto sum = std::get_if<Adder<T>>(&effects[e].config)) {
        result += sum->process(result);
      } else if (auto pipe = std::get_if<Piper<T>>(&effects[e].config)) {
        result += pipe->process(result);
      }
      result /= effects.size();
    }
    return result;
  }
  std::vector<Effect<T>> effects;
};

template <typename T> class Piper {
public:
  Piper(std::vector<std::vector<Effect<T>>> pipes, std::vector<float> mix)
      : pipes(pipes), mix(mix) {
    assert(pipes.size() == mix.size());
  }
  T process(T x) {
    T final_result = 0;
    size_t size = pipes.size();
    for (int p = 0; p < pipes.size(); p++) {
      T result = x;
      auto &effects = pipes[p];
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
      final_result += result * mix[p];
    }
    return final_result;
  }
  std::vector<std::vector<Effect<T>>> pipes;
  std::vector<float> mix;
};

namespace PresetEffects {
Effect<float> syntheticReverb(float dry, float wet);
}

#endif
