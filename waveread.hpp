#ifndef KEYBOARD_WAVEREAD_HPP
#define KEYBOARD_WAVEREAD_HPP
struct WavHeader {
  char riff[4];
  int32_t chunkSize;
  char wave[4];
  char fmt[4];
  int32_t subChunkSize;
  int16_t audioFormat;
  int16_t numChannels;
  int32_t sampleRate;
  int32_t byteRate;
  int16_t blockAlign;
  int16_t bitsPerSample;
  char data[4];
  int32_t dataSize;
};

std::vector<char> readWavData(const std::string &filename, int &sampleRate);

#endif
