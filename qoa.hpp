#ifndef QOA_HPP
#define QOA_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

typedef struct QOAFileHeader {
  char magic[4];
  uint32_t samples;
} QOAFileHeader;

typedef struct FrameHeader {
  uint8_t num_channels;
  uint8_t samplerate[3];
  uint16_t frame_samples;
  uint16_t frame_size;
} QOAFrameHeader;

typedef struct QOALMSState {
  int16_t history[4]; // most recent last
  int16_t weights[4]; // most recent last
} QOALMSState;

typedef struct QOASlice {
  uint64_t sliceData;
} QOASlice;

class QOA {
public:
  QOA(){};
  std::vector<short> loadFile(std::string file, int &nbrChannels,
                              int &sampleRate);
};

#endif
