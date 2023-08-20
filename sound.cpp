#include "sound.hpp"
#include "note.hpp"
#include <cmath>
#include <ncurses.h>
#include <thread>

const float PI = 3.14159265358979323846f;

std::string Sound::typeOfWave(Sound::WaveForm form) {
  switch (form) {
  case Sine:
    return "sine";
  case Triangular:
    return "triangular";
  case Square:
    return "square";
  case Saw:
    return "saw";
  }
  return "none";
}

std::vector<short> Sound::generateWave(Sound::WaveForm form, Note &note,
                                       ADSR &adsr) {
  switch (form) {
  case Sound::WaveForm::Sine:
    return Sound::generateSineWave(note, adsr);
  case Sound::WaveForm::Triangular:
    return Sound::generateTriangularWave(note, adsr);
  case Sound::WaveForm::Square:
    return Sound::generateSquareWave(note, adsr);
  case Sound::WaveForm::Saw:
    return Sound::generateSawWave(note, adsr);
  }

  return {};
}

static float square(float f) {
  f = fmod(f, 2.0 * PI);
  if (f < PI) {
    return 1.0;
  }
  return -1.0;
}

static float triangular(float f) {
  f = fmod(f, 2.0 * PI);
  if (f < PI / 2.0) {
    return f / (PI / 2.0);
  } else if (f < PI + PI / 2) {
    return 1 - (f - PI / 2) / (PI / 2);
  } else {
    return (f - (PI + PI / 2)) / (PI / 2) - 1;
  }
  return 1.0;
}

static float saw(float f) {
  f = fmod(f, 2.0 * PI);
  if (f < PI / 2.0) {
    return f / (PI / 2.0);
  } else if (f < PI + PI / 2) {
    return (f - PI / 2) / (PI / 2) - 1;
  } else {
    return (f - (PI + PI / 2)) / (PI / 2) - 1;
  }
  return 1.0;
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

std::vector<short> Sound::generateSquareWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] = static_cast<short>(
        adsr.response(i) * square(2.0 * PI * note.frequency * i * deltaT));
  }

  return samples;
}

std::vector<short> Sound::generateTriangularWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] = static_cast<short>(
        adsr.response(i) * triangular(2.0f * PI * note.frequency * i * deltaT));
  }

  return samples;
}

std::vector<short> Sound::generateSawWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] = static_cast<short>(
        adsr.response(i) * saw(2.0f * PI * note.frequency * i * deltaT));
  }

  return samples;
}
