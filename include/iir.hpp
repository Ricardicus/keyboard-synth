#ifndef KEYBOARD_IIR_HPP
#define KEYBOARD_IIR_HPP

#include <vector>

class IIR {
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
  short process(short in);
  short peek();

  int memory;
  std::vector<short> memoryX;
  std::vector<short> memoryY;
  std::vector<double> as;
  std::vector<double> bs;
};

namespace IIRFilters {
IIR lowPass(int sampleRate, float cutoffFreq);
IIR highPass(int sampleRate, float cutoffFreq);
IIR bandPass(int sampleRate, float centerFreq, float bandwidth);
}; // namespace IIRFilters

#endif
