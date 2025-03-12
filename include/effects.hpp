#ifndef KEYBOARD_EFFECTS_HPP
#define KEYBOARD_EFFECTS_HPP

#include <map>
#include <vector>

#include "fir.hpp"

class Effects {
public:
  Effects() {}
  Effects(std::vector<FIR> &firs_) { this->firs = firs_; }

  void addFIR(FIR &fir) { this->firs.push_back(fir); }
  std::vector<short> apply(std::vector<short> &buffer);
  std::vector<short> apply(std::vector<short> &buffer, size_t maxLen);

  std::vector<FIR> firs;

private:
  using Complex = std::complex<double>;
};

#endif
