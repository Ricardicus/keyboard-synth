#include "qoa.hpp"

#include <cmath>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint64_t swapEndianness(uint64_t value) {
    value = ((value & 0x00000000FFFFFFFF) << 32) | ((value & 0xFFFFFFFF00000000) >> 32);
    value = ((value & 0x0000FFFF0000FFFF) << 16) | ((value & 0xFFFF0000FFFF0000) >> 16);
    value = ((value & 0x00FF00FF00FF00FF) << 8) | ((value & 0xFF00FF00FF00FF00) >> 8);
    return value;
}

void check(size_t readBytes, size_t expected, const char *file,
           const char *msg) {
  if (readBytes == expected)
    return;
  fprintf(stderr, "error reading %s: %s", file, msg);
  fprintf(stderr, "expected %zu bytes, read %zu\n", expected, readBytes);
  exit(0);
}

std::vector<int> parseQOASlice(QOASlice &slice, int &sf_quant) {
  std::vector<int> qrNN;
  int offset = 0;

  // read quant (4 bits)
  offset = 4;
  sf_quant = slice.sliceData >> (64 - offset);

  int samples = 20;
  int sample_count = 0;
  while (sample_count < samples) {
    offset += 3;
    qrNN.push_back((slice.sliceData >> (64 - offset)) & 0b111);
    ++sample_count;
  }

  return qrNN;
}

std::vector<short> QOA::loadFile(std::string file, int &nbrChannels,
                                 int &sampleRate) {
  std::vector<short> interleaved;
  double dequant_tab[8] = {0.75, -0.75, 2.5, -2.5, 4.5, -4.5, 7, -7};

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
  header.samples =
      ((header.samples & 0xFF) << 24) | ((header.samples & 0xFF00) << 8) |
      ((header.samples >> 8) & 0xFF00) | ((header.samples >> 24) & 0xFF);
  printf("QOAFileHeader:\n");
  printf("  magic:   %c%c%c%c\n", header.magic[0], header.magic[1],
         header.magic[2], header.magic[3]);
  printf("  samples: %u\n", header.samples);

  // Frames
  double divisionResult = static_cast<double>(header.samples) / (256.0 * 20.0);
  uint16_t numberOfFrames = header.samples / (256 * 20);

  printf("Number of frames: %u\n", numberOfFrames);

  // Proceeding to read frame by frame
  uint16_t frameCount = 0;
  while (frameCount < numberOfFrames) {
    QOAFrameHeader frameHeader;

    // Read frame header
    size_t bytesRead = fread(&frameHeader, 1, sizeof(QOAFrameHeader), fp);
    check(bytesRead, sizeof(QOAFrameHeader), file.c_str(),
          "error: Failed to read QOAFileFrameHeader\n");

    sampleRate = frameHeader.samplerate[0] * (256 * 256) +
                 frameHeader.samplerate[1] * 256 + frameHeader.samplerate[2];

    printf("QOAFrameHeader # (%u/%u):\n", frameCount, numberOfFrames);
    printf("  num_channels: %u\n", frameHeader.num_channels);
    printf("  samplerate: %u\n", sampleRate);
    printf("  frame_samples: %u %u\n", frameHeader.frame_samples & 0xFF,
           frameHeader.frame_samples >> 8 & 0xFF);
    printf("  frame_size: %u\n", frameHeader.frame_size);

    nbrChannels = frameHeader.num_channels;

    QOALMSState lmsState[frameHeader.num_channels];

    uint8_t channel_count = 0;
    // Proceed by reading QOALMState for each channel
    while (channel_count < frameHeader.num_channels) {
      size_t bytesRead = fread(&lmsState, 1, sizeof(QOALMSState), fp);
      check(bytesRead, sizeof(QOALMSState), file.c_str(),
            "error: Failed to read QOALMSState\n");

      // process the lmsState so that it is in little endian format
      // THIS IS NOT PORTABLE.
      for (int n = 0; n < 4; ++n) {
        lmsState[channel_count].history[n] =
            (lmsState[channel_count].history[n] & 0xFF) * 256 +
            ((lmsState[channel_count].history[n] >> 8) & 0xFF);
        lmsState[channel_count].weights[n] =
            (lmsState[channel_count].weights[n] & 0xFF) * 256 +
            ((lmsState[channel_count].weights[n] >> 8) & 0xFF);
      }

      ++channel_count;
    }

    std::vector<short> frameSamples[frameHeader.num_channels];

    channel_count = 0;
    // Proceed by reading slices for each channel
    while (channel_count < frameHeader.num_channels) {
      QOASlice qoaSlice[256];

      uint16_t samples = (frameHeader.frame_samples & 0xFF) * 256 +
                         ((frameHeader.frame_samples >> 8) & 0xFF);
      uint16_t slices = samples / 20;
      printf("slices: %u, samples: %u, frame_samples[0]: %u, frame_samples[1]: "
             "%u\n",
             slices, samples, frameHeader.frame_samples & 0xFF,
             frameHeader.frame_samples >> 8);
      size_t bytesRead = fread(&qoaSlice, 1, sizeof(QOASlice) * slices, fp);
      check(bytesRead, sizeof(QOASlice) * slices, file.c_str(),
            "error: Failed to read QOASlice\n");
      printf("QOASlice read\n");

      int sf_quant;
      double sf = std::round(std::pow(sf_quant + 1, 2.75));
      for (int qq = 0; qq < slices; qq++) {
        //qoaSlice[qq].sliceData = 
        std::vector<int> qrNN = parseQOASlice(qoaSlice[qq], sf_quant);
        printf("qrNN.size(): %lu\n", qrNN.size());
        // Lets try to parse the samples
        int sample_count = 0;
        while (sample_count < qrNN.size()) {
          int qr = qrNN[sample_count];

          double rd = sf * dequant_tab[qr];
          if (rd < 0) {
            rd = std::ceil(rd - 0.5);
          } else {
            rd = std::floor(rd + 0.5);
          }
          int16_t r = static_cast<int16_t>(rd);

          int16_t p = 0;
          for (int n = 0; n < 4; ++n) {
            p += lmsState[channel_count].history[n] +
                 lmsState[channel_count].weights[n];
          }
          p >>= 13;

          int16_t s = static_cast<int16_t>(p + r);

          frameSamples[channel_count].push_back(s);

          // Update weights
          int16_t delta = r >> 4;
          for (int n = 0; n < 4; ++n) {
            lmsState[channel_count].weights[n] +=
                lmsState[channel_count].weights[n] < 0 ? -delta : delta;
          }

          // Update history
          for (int n = 0; n < 3; ++n) {
            lmsState[channel_count].history[n] =
                lmsState[channel_count].history[n + 1];
          }
          lmsState[channel_count].history[3] = s;

          ++sample_count;
        }

        printf("  sf_quant: %d\n", sf_quant);
        printf("      qr00: %d\n", qrNN[0]);
        printf("      qr01: %d\n", qrNN[1]);
        printf("      qr02: %d\n", qrNN[2]);
        printf("      ....\n");
        exit(0);
        qq++;
      }

      // exit(0);
      ++channel_count;
    }

    if (frameHeader.num_channels == 1) {
      int c = 0;
      while (c < frameSamples[0].size()) {
        interleaved.push_back(static_cast<short>(frameSamples[0][c]));
        c++;
      }
    } else if (frameHeader.num_channels == 2) {
      int c = 0;
      while (c < frameSamples[0].size()) {
        interleaved.push_back(static_cast<short>(frameSamples[0][c]));
        interleaved.push_back(static_cast<short>(frameSamples[1][c]));
        c++;
      }
    } else {
      fprintf(stderr,
              "Sorry, this number of channels I cannot deal with properly..");
    }
    ++frameCount;
  }

  printf("Total number of samples from file: %lu\n", header.samples);
  printf("Total number of samples parsed: %lu\n", interleaved.size());

  return interleaved;
}
