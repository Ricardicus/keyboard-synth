#ifndef KEYBOARD_IIR_HPP
#define KEYBOARD_IIR_HPP

#include <cmath>
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

  int memory;
  std::vector<T> memoryX;
  std::vector<T> memoryY;
  std::vector<double> as;
  std::vector<double> bs;
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
