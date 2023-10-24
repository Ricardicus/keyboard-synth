#include "qoa.hpp"

#include <cmath>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void check(size_t readBytes, size_t expected, const char *file,
           const char *msg) {
  if (readBytes == expected)
    return;
  fprintf(stderr, "error reading %s: %s", file, msg);
  exit(0);
}

std::vector<short> QOA::loadFile(std::string file) {
  QOAFileHeader header;
  FILE *fp = fopen(file.c_str(), "rb");
  if (fp == NULL) {
    fprintf(stderr, "error: failed to open file %s\n", file.c_str());
    return {};
  }

  // Read QOA file header
  size_t bytesRead = fread(&header, 1, sizeof(QOAFileHeader), fp);
  check(bytesRead, sizeof(QOAFileHeader), file.c_str(),
        "error: Failed to read QOAFileHeader\n");

  printf("QOAFileHeader:\n");
  printf("  magic:   %c%c%c%c\n", header.magic[0], header.magic[1],
         header.magic[2], header.magic[3]);
  printf("  samples: %u\n", header.samples);

  // Frames
  double divisionResult = static_cast<double>(header.samples) / (256.0 * 20.0);
  uint16_t numberOfFrames = static_cast<uint16_t>(std::ceil(divisionResult));

  printf("Number of frames: %u\n", numberOfFrames);

  // Proceeding to read frame by frame
  uint16_t frameCount = 0;
  while (frameCount < numberOfFrames) {
    QOAFrameHeader frameHeader;

    // Read frame header
    size_t bytesRead = fread(&frameHeader, 1, sizeof(QOAFrameHeader), fp);
    check(bytesRead, sizeof(QOAFrameHeader), file.c_str(),
          "error: Failed to read QOAFileFrameHeader\n");

    printf("QOAFrameHeader:\n");
    printf("  num_channels: %u\n", frameHeader.num_channels);
    printf("  samplerate: %u\n", frameHeader.samplerate[0] * (256 * 256) +
                                     frameHeader.samplerate[1] * 256 +
                                     frameHeader.samplerate[2]);
    printf("  frame_samples: %u\n", frameHeader.frame_samples);
    printf("  frame_size: %u\n", frameHeader.frame_size);

    uint8_t channel_count = 0;
    // Proceed by reading QOALMState for each channel
    while (channel_count < frameHeader.num_channels) {
      QOALMSState lmsState;
      size_t bytesRead = fread(&lmsState, 1, sizeof(QOALMSState), fp);
      check(bytesRead, sizeof(QOALMSState), file.c_str(),
            "error: Failed to read QOALMSState\n");
      printf("QOALMSState read (unintelligible)\n");

      ++channel_count;
    }

    channel_count = 0;
    // Proceed by reading slices for each channel
    while (channel_count < frameHeader.num_channels) {
      QOASlice qoaSlice[256];
      size_t bytesRead = fread(&qoaSlice, 1, sizeof(QOASlice)*256, fp);
      check(bytesRead, sizeof(qoaSlice), file.c_str(),
            "error: Failed to read QOASlice\n");
      printf("QOASlice read (unintelligible)\n");

      ++channel_count;
    }

    exit(0);
    ++frameCount;
  }

  exit(0);

  return {};
}
