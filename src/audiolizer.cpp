#include "MidiFile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <filesystem>
#include <iostream>
#include <ncurses.h>
#include <optional>
#include <thread>
#include <vector>

#include <stdio.h>

#include <SDL2/SDL.h>

#include "adsr.hpp"
#include "effect.hpp"
#include "keyboardstream.hpp"
#include "waveread.hpp"

constexpr int BUFFER_SIZE = 512;
constexpr int TOP_COMPONENTS = 128;

// MIDI note number to frequency
float noteToFreq(int note) { return 440.0f * pow(2, (note - 69) / 12.0f); }

bool fileExists(const std::string &file) {
  return std::filesystem::exists(file);
}

struct AudioPlaybackData {
  const std::vector<short> *audioBuffer = nullptr;
  size_t playbackPosition = 0;
};

class PlaybackBuffer {
public:
  std::vector<short> buffer{0};
  int index{0};

  void fillBuffer(short *fillMe, int samples) {
    for (int i = 0; i < samples; i++) {
      fillMe[i] = this->buffer[this->index];
      this->index = (this->index + 1) % this->buffer.size();
    }
  }
};

void normalize(std::vector<short> &buffer) {
  // Step 1: Find the maximum absolute value
  short max_val = 0;
  for (size_t i = 0; i < buffer.size(); ++i) {
    if (std::abs(buffer[i]) > max_val) {
      max_val = std::abs(buffer[i]);
    }
  }

  // Step 2: Compute scaling factor
  if (max_val == 0) {
    return; // Avoid division by zero
  }
  float scale = 32767.0f / max_val;

  // Step 3: Apply scaling
  for (size_t i = 0; i < buffer.size(); ++i) {
    int scaled = static_cast<int>(buffer[i] * scale);
    if (scaled > 32767)
      scaled = 32767;
    if (scaled < -32768)
      scaled = -32768;
    buffer[i] = static_cast<short>(scaled);
  }
}

void audioCallback(void *userdata, Uint8 *stream, int len) {
  short *streamBuf = reinterpret_cast<short *>(stream);
  int samples = len / sizeof(short);
  PlaybackBuffer *pb = static_cast<PlaybackBuffer *>(userdata);
  pb->fillBuffer(streamBuf, samples);
}

void print_usage(char *argv0) { printf("usage: %s sound-file.wav\n", argv0); }

using Complex = std::complex<double>;
typedef struct AudiolizedComponent {
  int bin;
  Complex value;
} AudiolizedComponent;

typedef struct AudiolizedBit {
  std::vector<AudiolizedComponent> components;
} AudiolizedBit;

class AudiolizedSound {
public:
  std::vector<AudiolizedBit> bits;

  void print() const {
    for (size_t i = 0; i < bits.size(); ++i) {
      std::cout << "Bit " << i << ":\n";
      for (const auto &component : bits[i].components) {
        double magnitude = std::abs(component.value);
        std::cout << "  Bin: " << component.bin << ", Magnitude: " << magnitude
                  << " (Value: " << component.value.real() << " + "
                  << component.value.imag() << "i)\n";
      }
    }
  }

  std::vector<short> reconstruct(int buffer_size) const {
    std::vector<short> output;

    for (const auto &bit : bits) {
      // Reconstruct full spectrum
      std::vector<Complex> spectrum(buffer_size, Complex(0.0, 0.0));

      for (const auto &component : bit.components) {
        if (component.bin >= 0 && component.bin < buffer_size) {
          spectrum[component.bin] = component.value;
        }
      }

      // Perform inverse DFT
      std::vector<short> timeDomain = FourierTransform::IDFT(spectrum);

      // Append to output
      output.insert(output.end(), timeDomain.begin(), timeDomain.end());
    }

    return output;
  }
};

AudiolizedSound audiolize(std::vector<short> buffer, int buffer_size,
                          int top_components) {
  AudiolizedSound result;

  for (size_t i = 0; i < buffer.size(); i += buffer_size) {
    // Prepare chunk with zero-padding if needed
    std::vector<short> chunk(buffer_size, 0);
    size_t remaining = buffer.size() - i;
    size_t copySize = remaining < buffer_size ? remaining : buffer_size;
    std::copy(buffer.begin() + i, buffer.begin() + i + copySize, chunk.begin());

    // Perform DFT
    std::vector<std::complex<double>> spectrum =
        FourierTransform::DFT(chunk, true);

    // Find top components by magnitude
    std::vector<std::pair<int, double>> magnitudes;
    for (int bin = 0; bin < static_cast<int>(spectrum.size()); ++bin) {
      magnitudes.emplace_back(bin, std::abs(spectrum[bin]));
    }

    // Sort bins by magnitude, descending
    std::sort(magnitudes.begin(), magnitudes.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    // Create AudiolizedBit with top N components
    AudiolizedBit bit;
    for (int j = 0;
         j < top_components && j < static_cast<int>(magnitudes.size()); ++j) {
      int bin = magnitudes[j].first;
      std::complex<double> value = spectrum[bin];
      AudiolizedComponent component{bin, value};
      bit.components.push_back(component);
    }

    result.bits.push_back(bit);
  }

  return result;
}

void usage(const std::string &program_name) {
  std::cout
      << "Usage: " << program_name
      << " --components [number] --buffer-size [number] --input [input-file]\n"
      << "\nOptions:\n"
      << "  --components   Number of top components to use (required)\n"
      << "  --buffer-size  Buffer size to use (required)\n"
      << "  --input        Path to input file (required)\n"
      << "  --clip-start   Start of the sound (percent), default 0\n"
      << "  --components   End of the sound (percent), default 100\n"
      << std::endl;
}

struct Config {
  std::string file;
  int top_components = TOP_COMPONENTS;
  int buffer_size = BUFFER_SIZE;
  int clip_start = 0;
  int clip_stop = 100;
};

Config parseArgs(int argc, char *argv[]) {
  Config config;

  if (argc <= 1) {
    usage(argv[0]);
    std::exit(1);
  }

  for (int i = 1; i < argc; i += 2) {
    std::string arg = argv[i];

    if (arg == "--components" && i + 1 < argc) {
      config.top_components = std::stoi(argv[i + 1]);
    } else if (arg == "--buffer-size" && i + 1 < argc) {
      config.buffer_size = std::stoi(argv[i + 1]);
    } else if (arg == "--clip-start" && i + 1 < argc) {
      config.clip_start = std::stoi(argv[i + 1]);
    } else if (arg == "--clip-stop" && i + 1 < argc) {
      config.clip_stop = std::stoi(argv[i + 1]);
    } else if (arg == "--input" && i + 1 < argc) {
      config.file = argv[i + 1];
      printf("config.file: %s\n", config.file.c_str());
    } else if (arg == "--help") {
      usage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "Unknown or incomplete argument: " << arg << "\n"
                << std::endl;
      usage(argv[0]);
      std::exit(1);
    }
  }

  if (config.file.empty()) {
    std::cerr << "Error: --input [file] is required!\n" << std::endl;
    usage(argv[0]);
    std::exit(1);
  }

  if (!std::filesystem::exists(config.file)) {
    std::cerr << "Error: Input file does not exist: " << config.file << "\n"
              << std::endl;
    usage(argv[0]);
    std::exit(1);
  }

  return config;
}

int main(int argc, char *argv[]) {
  Config config;

  if (argc < 2) {
    fprintf(stderr, "error: no input file\n");
    print_usage(argv[0]);
    return -1;
  }

  config = parseArgs(argc, argv);

  std::string file = config.file;
  int channels;
  int sampleRate;
  int bps;
  int size;

  std::vector<short> buffer_left, buffer_right;
  char *data = loadWAV(file, channels, sampleRate, bps, size);
  printf("Loaded file: %s\nSize: %d B\nSample rate: %d\nChannels: %d\n",
         file.c_str(), size, sampleRate, channels);

  if (channels == 2) {
    splitChannels(data, size, buffer_left, buffer_right);
  } else {
    buffer_left = convertToVector(data, size);
    buffer_right = buffer_left;
  }

  printf("Samples: %d\n", buffer_left.size());

  AudiolizedSound audiolized_left =
      audiolize(buffer_left, config.buffer_size, config.top_components);
  AudiolizedSound audiolized_right =
      audiolize(buffer_right, config.buffer_size, config.top_components);

  if (config.clip_stop != 100 || config.clip_start > 0) {
    int start =
        static_cast<int>((static_cast<float>(config.clip_start) / 100.0) *
                         audiolized_left.bits.size());
    int end = static_cast<int>((static_cast<float>(config.clip_stop) / 100.0) *
                               audiolized_left.bits.size());
    std::vector<AudiolizedBit> bits_left, bits_right;

    for (int i = start; i <= end; i++) {
      bits_left.push_back(audiolized_left.bits[i]);
      bits_right.push_back(audiolized_right.bits[i]);
    }

    audiolized_left.bits = bits_left;
    audiolized_right.bits = bits_right;
  }
  // Trim the bits
  printf("Number of bits: %lu\n", audiolized_left.bits.size());

  std::vector<short> buffer_left_reconstructed =
      audiolized_left.reconstruct(config.buffer_size);
  std::vector<short> buffer_right_reconstructed =
      audiolized_right.reconstruct(config.buffer_size);

  std::vector<short> reconstructed;
  for (int i = 0; i < buffer_right_reconstructed.size(); i++) {
    reconstructed.push_back(buffer_right_reconstructed[i]);
    reconstructed.push_back(buffer_left_reconstructed[i]);
  }

  // Normalize the reconstructed sound (maximize)
  normalize(reconstructed);

  PlaybackBuffer pb;
  pb.buffer = reconstructed;

  SDL_AudioSpec desired, obtained;
  SDL_zero(desired);
  desired.freq = SAMPLERATE;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = config.buffer_size;
  desired.callback = audioCallback;
  desired.userdata = &pb;

  if (SDL_OpenAudio(&desired, &obtained) < 0) {
    std::cerr << "SDL_OpenAudio failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_PauseAudio(0); // Start audio

  std::this_thread::sleep_for(std::chrono::seconds(3));

  SDL_CloseAudio();
  SDL_Quit();

  return 0;
}
