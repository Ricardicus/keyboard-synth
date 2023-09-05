#ifndef KEYBOARD_WAVEREAD_HPP
#define KEYBOARD_WAVEREAD_HPP
#include <cstdint>
#include <cstdio>
#include <string>
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

struct RIFF_Header {
  char chunkID[4];
  long chunkSize; // size not including chunkSize or chunkID
  char format[4];
};

/*
 * Struct to hold fmt subchunk data for WAVE files.
 */
struct WAVE_Format {
  char subChunkID[4];
  long subChunkSize;
  short audioFormat;
  short numChannels;
  long sampleRate;
  long byteRate;
  short blockAlign;
  short bitsPerSample;
};

/*
 * Struct to hold the data of the wave file
 */
struct WAVE_Data {
  char subChunkID[4]; // should contain the word data
  long subChunk2Size; // Stores the size of the data block
};

char *loadWAV(std::string filename, int &chan, int &samplerate, int &bps,
              int &size);

std::vector<short> convertToVector(const char *data, int numSamples);

void splitChannels(const char *data, size_t dataSize, std::vector<short> &left,
                   std::vector<short> &right);

#endif
