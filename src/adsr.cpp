// turns out adsr.hpp has everything
#include "adsr.hpp"

std::string ADSR::getCoolASCIVisualization(const std::string &prefix) {
  std::string result;
  int dots_per_quant_y = 7;
  int dots_per_quant_x = 4;
  int dot_count = 0;
  int length = this->quantas * dots_per_quant_x;
  ADSR adsr_help(this->amplitude, this->qadsr[0], this->qadsr[1],
                 this->qadsr[2], this->qadsr[3],
                 this->sustain_level * 1.0 / this->amplitude, length);
  int max_val = 0;

  for (int x = 0; x < length; x++) {
    int val = adsr_help.response(x);
    if (val > max_val) {
      max_val = val;
    }
  }

  for (int doty = 0; doty < dots_per_quant_y; doty++) {
    float frac = (1.0 * dots_per_quant_y - 1 - doty) / (1.0 * dots_per_quant_y);
    float upper_level = max_val * frac;
    result += prefix;
    for (int x = 0; x < length; x++) {
      int current_val = adsr_help.response(x);
      if (current_val >= upper_level) {
        result += ".";

      } else {
        result += " ";
      }
    }
    result += "\n";
  }

  return result;
}
