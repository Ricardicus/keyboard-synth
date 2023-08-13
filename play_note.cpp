#include <chrono> // for std::chrono::seconds
#include <cmath>
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

const float PI = 3.14159265358979323846f;

// Generate sine wave data for a specific frequency
std::vector<short> generateSineWave(float frequency, float duration,
                                    int sampleRate) {
  int sampleCount = static_cast<int>(duration * sampleRate);
  std::vector<short> samples(sampleCount);

  float deltaT = 1.0f / sampleRate;
  for (int i = 0; i < sampleCount; i++) {
    samples[i] =
        static_cast<short>(32767.0f * sin(2.0f * PI * frequency * i * deltaT));
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
  float duration = 3.0f;  // 1 second
  std::vector<short> waveData = generateSineWave(261.63f, duration, sampleRate);

  // Create buffer and source
  ALuint buffer;
  alGenBuffers(1, &buffer);
  alBufferData(buffer, AL_FORMAT_MONO16, waveData.data(),
               waveData.size() * sizeof(short), sampleRate);

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
