#pragma once
#include "config.hpp"
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

// -----------------------------------------------------------------------------
// Looper: 4-track audio looper with metronome and adjustable BPM (4/4 time)
// Each loop has a fixed length in bars (default: 8 bars).
// -----------------------------------------------------------------------------
class Looper {
public:
  Looper();

  // --- Recording control ---
  void setRecording(bool enabled);
  bool isRecording() const;

  void setActiveTrack(int index);
  int getActiveTrack() const;

  void clearTrack(int index);

  // --- Loop length control ---
  void setNumBars(int bars);
  int getNumBars() const;

  // --- Metronome control ---
  void setBPM(float bpm);
  float getBPM() const;
  void enableMetronome(bool enable);
  bool isMetronomeEnabled() const;
  bool setMetronomeSampler(const std::string &waveFileHigh,
                           const std::string &waveFileLow);
  void setMetronomeVolume(float v) { metronomeVolume_ = v; }

  // --- Audio processing ---
  // Called from the audio callback to fill the output buffer
  void fillBuffer(float *buffer, const int len);
  float update(float input);

  void toggleRecording() { this->recording_ = !this->recording_; }

private:
  struct Track {
    std::vector<float> data;
    std::size_t writePos = 0;
    bool armed = false;
  };

  std::vector<Track> tracks_;
  int activeTrack_;
  bool recording_ = false;

  // --- Loop timing ---
  int numBars_;                           // length of loop in bars (default: 8)
  static constexpr int BEATS_PER_BAR = 4; // 4/4 time
  std::size_t loopLengthSamples_;         // computed total length in samples

  // --- Metronome ---
  float bpm_;
  bool metronomeEnabled_;
  double metronomePhase_;
  double metronomeIncrement_;
  float metronomeVolume_;
  std::vector<float> metronomeSamplesHigh_;
  std::vector<float> metronomeSamplesLow_;
  bool metronomeUseSampler_ = false;

  std::mutex mtx_; // protect track modifications

  // --- Helpers ---
  void updateMetronomeIncrement();
  void updateLoopLength();
  float generateMetronomeSample();
};
