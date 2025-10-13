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

  // ---- MetronomeBPM ----
  void setMetronomeBPM(int bpm) { metronomeBpm_ = bpm; }
  int getMetronomeBPM() const { return metronomeBpm_; }
  // ---- MetronomeFloat ----
  void setMetronomeVolume(float vol) { metronomeVolume_ = vol; }
  float getMetronomeVolume() const { return metronomeVolume_; }

  // Tracker
  void setNumTracks(int tracks) { numTracks_ = tracks; }
  int getNumTracks() { return numTracks_; }
  void setNumBars(int bars) { numBars_ = bars; }
  int getNumBars() { return numBars_; }

private:
  // Private constructor initializes defaults
  Config()
      : sampleRate_(44100), // default = 44100 Hz
        bufferSize_(206),   // default = 512 frames
        metronomeBpm_(100), metronomeVolume_(0.25f), numTracks_(4) {}

  int sampleRate_;
  std::size_t bufferSize_;
  int metronomeBpm_;
  float metronomeVolume_;
  int numTracks_ = 4;
  int numBars_ = 8;
};
