#ifndef KEYBOARD_IIR_HPP
#define KEYBOARD_IIR_HPP

#include <cmath>
#include <json.hpp>
#include <optional>
#include <vector>

template <typename T> class IIR {
public:
  // Constructor stores 'memory' and initializes vectors to the desired size
  IIR(int memory) : memory(memory) { clear(); }

  // Reset vectors to a known state (all zeros)
  void clear() {
    memoryX.clear();
    memoryY.clear();
    memoryX.resize(memory, 0);
    memoryY.resize(memory, 0);
  }

  // Set the zeros coefficients (as)
  void setAs(const std::vector<double> &newAs) { as = newAs; }

  // Set the poles coefficients (bs)
  void setBs(const std::vector<double> &newBs) { bs = newBs; }

  // Example processing methods (to be implemented)
  T process(T in);
  T peek();

  nlohmann::json toJson() const {
    return nlohmann::json{{"memory", memory},
                          {"as", as},
                          {"bs", bs},
                          {"presentable", presentable},
                          {"bypass", bypass}};
  }

  static std::optional<IIR<T>> fromJson(const nlohmann::json &j) {
    if (!j.contains("memory") || !j["memory"].is_number_integer()) {
      return std::nullopt;
    }

    IIR<T> iir(j["memory"].get<int>());

    if (j.contains("as") && j["as"].is_array()) {
      iir.setAs(j["as"].get<std::vector<double>>());
    }

    if (j.contains("bs") && j["bs"].is_array()) {
      iir.setBs(j["bs"].get<std::vector<double>>());
    }

    if (j.contains("presentable") && j["presentable"].is_number()) {
      iir.presentable = j["presentable"].get<float>();
    }

    if (j.contains("bypass") && j["bypass"].is_boolean()) {
      iir.bypass = j["bypass"].get<bool>();
    }

    return iir;
  }

  int memory;
  std::vector<T> memoryX;
  std::vector<T> memoryY;
  std::vector<double> as;
  std::vector<double> bs;
  float presentable = 0;
  bool bypass = false;
};

namespace IIRFilters {

template <typename T> IIR<T> lowPass(int sampleRate, float cutoffFreq) {
  double fs = static_cast<double>(sampleRate);
  double fc = cutoffFreq / fs;
  double K = tan(M_PI * fc);
  double K2 = K * K;
  double Q = 1.0 / sqrt(2.0);
  double norm = 1.0 / (1.0 + K / Q + K2);

  double b0 = K2 * norm;
  double b1 = 2.0 * b0;
  double b2 = b0;

  double a1 = 2.0 * (K2 - 1.0) * norm;
  double a2 = (1.0 - K / Q + K2) * norm;

  std::vector<double> bs = {b0, b1, b2};
  std::vector<double> as = {a1, a2, 0.0};

  IIR<T> filter(3);
  filter.setAs(as);
  filter.setBs(bs);

  filter.presentable = cutoffFreq;
  return filter;
}

template <typename T> IIR<T> highPass(int sampleRate, float cutoffFreq) {
  double fs = static_cast<double>(sampleRate);
  double fc = cutoffFreq / fs;
  double K = tan(M_PI * fc);
  double K2 = K * K;
  double Q = 1.0 / sqrt(2.0);
  double norm = 1.0 / (1.0 + K / Q + K2);

  double b0 = 1.0 * norm;
  double b1 = -2.0 * norm;
  double b2 = b0;

  double a1 = 2.0 * (K2 - 1.0) * norm;
  double a2 = (1.0 - K / Q + K2) * norm;

  std::vector<double> bs = {b0, b1, b2};
  std::vector<double> as = {a1, a2, 0.0};

  IIR<T> filter(3);
  filter.setAs(as);
  filter.setBs(bs);

  filter.presentable = cutoffFreq;
  return filter;
}

template <typename T>
IIR<T> bandPass(int sampleRate, float centerFreq, float bandwidth) {
  double fs = static_cast<double>(sampleRate);
  double fc = centerFreq / fs;
  double K = tan(M_PI * fc);
  double Q = centerFreq / bandwidth;
  double K2 = K * K;
  double norm = 1.0 / (1.0 + K / Q + K2);

  double b0 = (K / Q) * norm;
  double b1 = 0.0;
  double b2 = -b0;

  double a1 = 2.0 * (K2 - 1.0) * norm;
  double a2 = (1.0 - K / Q + K2) * norm;

  std::vector<double> bs = {b0, b1, b2};
  std::vector<double> as = {a1, a2, 0.0};

  IIR<T> filter(3);
  filter.setAs(as);
  filter.setBs(bs);
  return filter;
}

} // namespace IIRFilters

#endif
