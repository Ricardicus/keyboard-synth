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
  case WaveFile:
    return "file";
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
  case Sound::WaveForm::WaveFile:
    break;
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
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           sin(2.0f * PI * note.frequency * i * deltaT));
  }

  return samples;
}

std::vector<short> Sound::generateSquareWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           square(2.0 * PI * note.frequency * i * deltaT));
  }

  return samples;
}

std::vector<short> Sound::generateTriangularWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           triangular(2.0f * PI * note.frequency * i * deltaT));
  }

  return samples;
}

std::vector<short> Sound::generateSawWave(Note &note, ADSR &adsr) {
  int sampleCount = note.length;
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / note.sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(adsr.response(i) * note.volume *
                           saw(2.0f * PI * note.frequency * i * deltaT));
  }

  return samples;
}

std::vector<short> Sound::generateWave(Rank &rank) {
  int sampleCount = 0;
  for (const auto &pipe : rank.pipes) {
    if (pipe.first.length > sampleCount) {
      sampleCount = pipe.first.length;
    }
  }
  std::vector<short> samples(sampleCount);

  for (int i = 0; i < sampleCount; i++) {
    for (const Pipe &pipe : rank.pipes) {
      Note note = pipe.first;
      Sound::WaveForm form = pipe.second;
      float deltaT = 1.0f / note.sampleRate;

      switch (form) {
      case Sound::WaveForm::Sine:
        samples[i] +=
            static_cast<short>(rank.adsr.response(i) * note.volume *
                               sin(2.0f * PI * note.frequency * i * deltaT));
        break;
      case Sound::WaveForm::Triangular:
        samples[i] += static_cast<short>(
            rank.adsr.response(i) * note.volume *
            triangular(2.0f * PI * note.frequency * i * deltaT));
        break;
      case Sound::WaveForm::Square:
        samples[i] +=
            static_cast<short>(rank.adsr.response(i) * note.volume *
                               square(2.0f * PI * note.frequency * i * deltaT));
        break;
      case Sound::WaveForm::Saw:
        samples[i] +=
            static_cast<short>(rank.adsr.response(i) * note.volume *
                               saw(2.0f * PI * note.frequency * i * deltaT));
        break;
      case Sound::WaveForm::WaveFile:
        break;
      }
    }
  }

  return samples;
}

Sound::Rank Sound::Rank::superSaw(float frequency, int length, int sampleRate) {
  Rank rank;

  float detune = 0.5;
  float detune_cents[] = {-12.0, -8.0, -4.0, 0.0, 4.0, 8.0, 12.0};
  const int num_oscillators = sizeof(detune_cents) / sizeof(detune_cents[0]);

  for (int i = 0; i < num_oscillators; i++) {
    float detuned_freq =
        frequency * powf(2.0f, detune_cents[i] * detune / 1200.0f);
    Note note(detuned_freq, length, sampleRate);
    note.volume = 1.0 / num_oscillators;
    Pipe pipe(note, Sound::WaveForm::Saw);
    rank.addPipe(pipe);
  }

  return rank;
}
