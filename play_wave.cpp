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
#include "effects.hpp"
#include "fir.hpp"
#include "waveread.hpp"

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

int main(int argc, char *argv[]) {

  if (argc < 2) {
    fprintf(stderr, "usage: %s path/to/wave.wav\n", argv[0]);
    return 1;
  }

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
  ALuint buffer;
  alGenBuffers(1, &buffer);
  ALenum format = AL_FORMAT_MONO8;
  int channels, sampleRate, bps, size;
  char *data = loadWAV(argv[1], channels, sampleRate, bps, size);
  printf("channels: %d, sampleRate: %d, bps: %d, data size: %d\n", channels,
         sampleRate, bps, size);

  if (channels == 1) {
    if (bps == 8) {
      format = AL_FORMAT_MONO8;
    } else {
      format = AL_FORMAT_MONO16;
    }
  } else {
    if (bps == 8) {
      format = AL_FORMAT_STEREO8;
    } else {
      format = AL_FORMAT_STEREO16;
    }
  }

  alBufferData(buffer, format, data, size, sampleRate);

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
