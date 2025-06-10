#ifndef KEYBOARD_FIR_HPP
#define KEYBOARD_FIR_HPP

#include "dft.hpp"
#include "waveread.hpp"
#include <algorithm>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <json.hpp>
class FIR {
public:
  FIR(std::vector<short> &buffer_, int sampleRate_) {
    this->buffer = buffer_;
    this->sampleRate = sampleRate_;
    this->normalize = false;
  }

  FIR(int sampleRate_) { this->sampleRate = sampleRate_; }

  void setBuffer(std::vector<short> buffer_) { this->buffer = buffer_; }
  void setResonance(std::vector<float> alphas, float seconds);
  void setIR(std::vector<float> impulseResponse_) {
    this->impulseResponse = impulseResponse_;
  }
  bool loadFromFile(std::string file);
  short calc(int index);
  void setNormalization(bool normalize) { this->normalize = normalize; };
  bool getNormalization() { return this->normalize; };
  std::vector<short> convolute(int max_size);
  std::vector<float> getIR() { return this->impulseResponse; };
  size_t getIRLen() { return this->impulseResponse.size(); };

  nlohmann::json toJson() const {
    return nlohmann::json{{"impulseResponse", this->impulseResponse},
                          {"sampleRate", this->sampleRate},
                          {"normalize", this->normalize}};
  };

  static std::optional<FIR> fromJson(const nlohmann::json &j) {
    if (!j.contains("sampleRate") || !j["sampleRate"].is_number_integer()) {
      return std::nullopt;
    }

    FIR fir(j["sampleRate"].get<int>());

    if (j.contains("buffer") && j["buffer"].is_array()) {
      fir.setBuffer(j["buffer"].get<std::vector<short>>());
    }

    if (j.contains("impulseResponse") && j["impulseResponse"].is_array()) {
      fir.setIR(j["impulseResponse"].get<std::vector<float>>());
    }

    if (j.contains("normalize") && j["normalize"].is_boolean()) {
      fir.setNormalization(j["normalize"].get<bool>());
    }

    return fir;
  };

private:
  float ir(int index);
  short buf(int index);
  std::vector<short> buffer;
  std::vector<float> impulseResponse;
  int sampleRate;
  bool normalize;
};

#endif
