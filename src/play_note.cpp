#include <chrono> // for std::chrono::seconds
#include <cmath>
#include <complex>
#include <iostream>
#include <thread> // for std::this_thread::sleep_for
#include <vector>

#ifdef USE_OPENAL_PREFIX
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#elif USE_AL_PREFIX
#include <AL/al.h>
#include <AL/alc.h>
#else
#include <OpenAL/al.h>  // Guessing. Use the definitions above for compilation.
#include <OpenAL/alc.h> // Guessing. Use the definitions above for compilation.
#endif

#include "adsr.hpp"
#include "dft.hpp"
#include "fir.hpp"
#include "effects.hpp"

const float PI = 3.14159265358979323846f;
using Complex = std::complex<double>;

// Generate sine wave data for a specific frequency
std::vector<short> generateSineWave(float frequency, ADSR &adsr, float duration,
                                    int sampleRate) {
  int sampleCount = static_cast<int>(duration * sampleRate);
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] = static_cast<short>(adsr.response(i) *
                                    sin(2.0f * PI * frequency * i * deltaT));
  }

  return samples;
}

int main() {
  // Initialize OpenAL
  ALCdevice *device = alcOpenDevice(NULL);
  if (!device) {
    std::cerr << "Failed to open device!" << std::endl;
    return -1;
  }

  ALCcontext *context = alcCreateContext(device, NULL);
  if (!context || !alcMakeContextCurrent(context)) {
    std::cerr << "Failed to create or set context!" << std::endl;
    alcCloseDevice(device);
    return -1;
  }

  // Generate C4 sine wave
  int sampleRate = 44100; // 44.1 kHz
  float duration = 1.0f;  // 1 second
  short amplitude = 32767;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(sampleRate * duration));

  std::vector<short> waveData =
      generateSineWave(261.63f, adsr, duration, sampleRate);

  FIR fir1(sampleRate);
  fir1.setResonance({1.0, 0.8, 0.6, 0.4, 0.2, 0.1}, 1.0);
  FIR fir2(sampleRate);
  fir2.setResonance({0.5, 0.4, 0.3, 0.2, 0.1}, 0.5);

  Effects effects;
  effects.addFIR(fir1);
  effects.addFIR(fir2);
  
  std::vector<short> result = effects.apply(waveData);

  // Create buffer and source
  ALuint buffer;
  alGenBuffers(1, &buffer);
  alBufferData(buffer, AL_FORMAT_MONO16, result.data(),
               result.size() * sizeof(short), sampleRate);

  ALuint source;
  alGenSources(1, &source);
  alSourcei(source, AL_BUFFER, buffer);

  // Play source
  alSourcePlay(source);
  while (true) {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
      break; // Exit loop if source is no longer playing
    }

    ALint sampleOffset = 0;
    alGetSourcei(source, AL_SAMPLE_OFFSET, &sampleOffset);
    std::cout << "Currently playing at sample: " << sampleOffset << std::endl;

    // Sleep for a short duration before checking again
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Cleanup
  alDeleteSources(1, &source);
  alDeleteBuffers(1, &buffer);

  alcMakeContextCurrent(NULL);
  alcDestroyContext(context);
  alcCloseDevice(device);

  return 0;
}
