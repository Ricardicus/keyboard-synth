#include "iir.hpp"

#include <algorithm>
#include <cmath>

short IIR::peek() {
  if (this->memoryY.size() > 0) {
    return this->memoryY[0];
  }
  return 0;
}

short IIR::process(short in) {
  if (this->memoryY.size() == 0) {
    return 0;
  }
  std::rotate(this->memoryY.rbegin(), this->memoryY.rbegin() + 1,
              this->memoryY.rend());
  std::rotate(this->memoryX.rbegin(), this->memoryX.rbegin() + 1,
              this->memoryX.rend());
  this->memoryX[0] = in;

  double val = 0;
  for (int i = 0; i < this->as.size(); i++) {
    val += this->bs[i] * this->memoryX[i];
    if (i != this->as.size() - 1) {
      val -= this->as[i] * this->memoryY[i + 1];
    }
  }
  this->memoryY[0] = static_cast<short>(val);

  return this->memoryY[0];
}

IIR IIRFilters::lowPass(int sampleRate, float cutoff) {
  double fs = static_cast<double>(sampleRate);
  float fc = cutoff * fs;
  double K = tanf(M_PI * fc / fs);
  double K2 = K * K;
  double Q = 1.0 / sqrtf(2);
  double norm = 1.0 / (1.0 + K / Q + K2);

  double b0 = K2 * norm;
  double b1 = 2 * b0;
  double b2 = b0;

  double a1 = 2 * (K2 - 1) * norm;
  double a2 = (1 - K / Q + K2) * norm;

  std::vector<double> bs = {b0, b1, b2};
  std::vector<double> as = {a1, a2, 0};

  IIR lowPass(3);
  lowPass.setAs(as);
  lowPass.setBs(bs);

  return lowPass;
}
