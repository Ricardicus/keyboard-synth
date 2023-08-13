#include "sound.hpp"
#include "note.hpp"
#include <cmath>
#include <thread>
#include <ncurses.h>

const float PI = 3.14159265358979323846f;

std::vector<short> Sound::generateWave(Sound::WaveForm form, Note &note,
                                       ADSR &adsr) {
  switch (form) {
  case Sound::WaveForm::Sine:
    return Sound::generateSineWave(note, adsr);
  }
  return {};
}

std::vector<short> Sound::generateSineWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] = static_cast<short>(
        adsr.response(i) * sin(2.0f * PI * note.frequency * i * deltaT));
  }

  return samples;
}

