#ifndef KEYBOARD_SOUND_HPP
#define KEYBOARD_SOUND_HPP

#include "adsr.hpp"
#include "note.hpp"
#include <vector>
namespace Sound {
enum WaveForm { Sine, Triangular, Square, Saw };

std::string typeOfWave(WaveForm form);
std::vector<short> generateWave(WaveForm f, Note &note, ADSR &adsr);
std::vector<short> generateSineWave(Note &note, ADSR &adsr);
std::vector<short> generateSquareWave(Note &note, ADSR &adsr);
std::vector<short> generateSawWave(Note &note, ADSR &adsr);
std::vector<short> generateTriangularWave(Note &note, ADSR &adsr);

} // namespace Sound

#endif
