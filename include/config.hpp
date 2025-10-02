// config.hpp
#pragma once
#include <cstddef>

class Config {
public:
  // Access the singleton instance
  static Config &instance() {
    static Config instance; // created on first use, thread-safe since C++11
    return instance;
  }

  // Delete copy and assignment (singleton should not be copied)
  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

  // ---- SampleRate ----
  void setSampleRate(int rate) { sampleRate_ = rate; }
  int getSampleRate() const { return sampleRate_; }

  // ---- BufferSize ----
  void setBufferSize(std::size_t size) { bufferSize_ = size; }
  std::size_t getBufferSize() const { return bufferSize_; }

private:
  // Private constructor initializes defaults
  Config()
      : sampleRate_(44100), // default = 44100 Hz
        bufferSize_(512)    // default = 512 frames
  {}

  int sampleRate_;
  std::size_t bufferSize_;
};
