#ifndef YIN_HPP
#define YIN_HPP

#include <algorithm>
#include <cmath>
#include <vector>

class YIN {
public:
  /// Constructor
  /// @param sampleRate - audio sample rate (Hz)
  /// @param minFreq    - lowest detectable frequency (Hz)
  /// @param maxFreq    - highest detectable frequency (Hz)
  /// @param threshold  - threshold for CMND (typical ~0.1–0.2)
  YIN(int sampleRate = 44100, float minFreq = 80.0f, float maxFreq = 8000.0f,
      float threshold = 0.1f)
      : sampleRate(sampleRate), minFreq(minFreq), maxFreq(maxFreq),
        threshold(threshold) {

    // Buffer length: at least 2 periods of lowest freq
    bufferSize = static_cast<int>(2.0f * sampleRate / minFreq);
    audioBuffer.reserve(bufferSize);
  }

  /// Add samples into the analysis buffer
  void addSamples(const float *buffer, const int len) {
    for (int i = 0; i < len; i++) {
      if ((int)audioBuffer.size() >= bufferSize) {
        audioBuffer.erase(audioBuffer.begin()); // sliding window
      }
      audioBuffer.push_back(buffer[i]);
    }
  }

  /// Compute YIN pitch frequency
  /// @return estimated f0 in Hz, or -1 if not enough samples
  float getYinFrequency() {
    if ((int)audioBuffer.size() < bufferSize)
      return -1.0f;

    int tauMin = static_cast<int>(sampleRate / maxFreq);
    int tauMax = static_cast<int>(sampleRate / minFreq);

    if (tauMax > bufferSize / 2)
      tauMax = bufferSize / 2;

    std::vector<float> diff(tauMax + 1, 0.0f);
    std::vector<float> cmnd(tauMax + 1, 0.0f);

    // Step 1: Difference function
    for (int tau = tauMin; tau <= tauMax; tau++) {
      for (int i = 0; i < tauMax; i++) {
        float delta = audioBuffer[i] - audioBuffer[i + tau];
        diff[tau] += delta * delta;
      }
    }

    // Step 2: Cumulative mean normalized difference
    cmnd[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = tauMin; tau <= tauMax; tau++) {
      runningSum += diff[tau];
      cmnd[tau] = (diff[tau] * tau) / (runningSum + 1e-9f);
    }

    // Step 3: Absolute threshold
    int tauEstimate = -1;
    for (int tau = tauMin; tau <= tauMax; tau++) {
      if (cmnd[tau] < threshold) {
        tauEstimate = tau;
        while (tau + 1 <= tauMax && cmnd[tau + 1] < cmnd[tau]) {
          tau++;
          tauEstimate = tau;
        }
        break;
      }
    }

    if (tauEstimate == -1) {
      return -1.0f; // no pitch found
    }

    // Step 4: Parabolic interpolation
    int tau = tauEstimate;
    if (tau > tauMin && tau < tauMax) {
      float s0 = cmnd[tau - 1];
      float s1 = cmnd[tau];
      float s2 = cmnd[tau + 1];
      float betterTau = tau + (s2 - s0) / (2 * (2 * s1 - s2 - s0));
      return sampleRate / betterTau;
    }

    return sampleRate / (float)tauEstimate;
  }

private:
  int sampleRate;
  float minFreq;
  float maxFreq;
  float threshold;
  int bufferSize;
  std::vector<float> audioBuffer;
};

#endif // YIN_HPP
