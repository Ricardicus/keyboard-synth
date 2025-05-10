#include "iir.hpp"

#include <algorithm>
#include <cmath>

template <> short IIR<short>::peek() {
  if (this->memoryY.size() > 0) {
    return this->memoryY[0];
  }
  return 0;
}

template <> short IIR<short>::process(short in) {
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

template <> float IIR<float>::peek() {
  if (this->memoryY.size() > 0) {
    return this->memoryY[0];
  }
  return 0;
}

template <> float IIR<float>::process(float in) {
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
  this->memoryY[0] = static_cast<float>(val);

  return this->memoryY[0];
}
