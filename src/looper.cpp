#include "looper.hpp"
#include "config.hpp"
#include "sound.hpp"
#include "waveread.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>

const float PI = 3.14159265358979323846f;

// --------------------------- Utilities ---------------------------

namespace {
inline std::size_t wrap_advance(std::size_t pos, std::size_t len) {
  ++pos;
  return (pos == len) ? 0 : pos;
}

inline float fastExpDecay(float x) {
  // x in [0,1], snappy decay for a click envelope
  return std::exp(-6.0f * x);
}
} // namespace

// --------------------------- Looper ------------------------------

Looper::Looper()
    : tracks_(Config::instance().getNumTracks()), activeTrack_(0),
      recording_(false), numBars_(8), bpm_(120.0f), metronomeEnabled_(false),
      metronomePhase_(0.0), metronomeIncrement_(0.0), metronomeVolume_(0.25f) {
  updateLoopLength();
  updateMetronomeIncrement();

  // Allocate per-track buffers to loop length
  for (auto &t : tracks_) {
    t.data.assign(loopLengthSamples_, 0.0f);
  }
}

// --- Recording control ---
void Looper::setRecording(bool enabled) {
  recording_ = enabled;
  if (tracks_.size() > this->activeTrack_) {
    tracks_[this->activeTrack_].recordingStartIdx = idx_;
    tracks_[this->activeTrack_].noInputYet = true;
  }
}

bool Looper::isRecording() const { return recording_; }

void Looper::setActiveTrack(int index) {
  if (index < 0)
    index = 0;
  if (index >= Config::instance().getNumTracks())
    index = Config::instance().getNumTracks() - 1;
  activeTrack_ = index;
}

int Looper::getActiveTrack() const { return activeTrack_; }

void Looper::clearTrack(int index) {
  if (index < 0 || index >= Config::instance().getNumTracks())
    return;
  auto &t = tracks_[index];
  t.data.assign(loopLengthSamples_, 0.0f);
}

// --- Loop length control ---
void Looper::setNumBars(int bars) {
  if (bars < 1)
    bars = 1;
  if (numBars_ == bars)
    return;
  numBars_ = bars;
  updateLoopLength();

  // Reallocate tracks to the new loop length (clear for simplicity)
  for (auto &t : tracks_) {
    t.data.assign(loopLengthSamples_, 0.0f);
  }
}

int Looper::getNumBars() const { return numBars_; }

// --- Metronome control ---
void Looper::setBPM(float bpm) {
  if (bpm < 0.0f)
    bpm = 0.0f;
  if (bpm_ == bpm)
    return;
  bpm_ = bpm;
}

float Looper::getBPM() const { return bpm_; }

void Looper::enableMetronome(bool enable) { metronomeEnabled_ = enable; }

bool Looper::isMetronomeEnabled() const { return metronomeEnabled_; }

// --- Helpers ---
void Looper::updateMetronomeIncrement() {
  const int sr = Config::instance().getSampleRate();
  metronomeIncrement_ =
      (sr > 0) ? (static_cast<double>(bpm_) / 60.0) / static_cast<double>(sr)
               : 0.0;
}

void Looper::updateLoopLength() {
  const int sr = Config::instance().getSampleRate();
  if (sr <= 0 || bpm_ <= 0.0f) {
    loopLengthSamples_ = 0;
    return;
  }
  const double secondsPerBeat = 60.0 / static_cast<double>(bpm_);
  const double totalSeconds = 30;
  loopLengthSamples_ = static_cast<std::size_t>(
      std::max(1.0, std::round(totalSeconds * static_cast<double>(sr))));
}

// Metronome: cheap click with short envelope on beat wrap
float Looper::generateMetronomeSample() {
  struct ClickEnv {
    float toneLengthOther = 0.025f;
    float toneLengthStart = 0.05f;
    int idx = 0;
    float phase = 0;
    // Unless we use sampler
    float freqLow = 880;
    float freqHigh = 1760;
    // If we use sampler
    int sampleIdx = 0;
  };
  static ClickEnv s;

  float result = 0;
  bool useStart = false;

  const int sr = Config::instance().getSampleRate();

  int indexSoundStart = sr * 60 / static_cast<int>(bpm_);
  int indexBar = indexSoundStart * numBars_;

  int soundLen;
  // We beat every 1 out of 4
  if ((s.idx % (4 * indexSoundStart)) < indexSoundStart) {
    useStart = true;
  }

  int idxNorm = s.idx % indexSoundStart;

  if (metronomeUseSampler_) {

    if (s.idx % indexSoundStart == 0) {
      s.sampleIdx = 0;
    }

    if (useStart) {
      soundLen = static_cast<int>(metronomeSamplesHigh_.size());
    } else {
      soundLen = static_cast<int>(metronomeSamplesLow_.size());
    }

    if (s.sampleIdx < soundLen) {
      if (useStart) {
        result = metronomeSamplesHigh_[s.sampleIdx];
      } else {
        result = metronomeSamplesLow_[s.sampleIdx];
      }
    }

    s.sampleIdx++;
  } else {
    float deltaT = 1.0f / Config::instance().getSampleRate();
    float add = 2.0 * PI * s.freqLow * deltaT;

    soundLen = static_cast<int>(s.toneLengthOther * static_cast<float>(sr));

    if (useStart) {
      soundLen = static_cast<int>(s.toneLengthStart * static_cast<float>(sr));
      add = 2.0 * PI * s.freqHigh * deltaT;
    }

    if (idxNorm < soundLen) {
      result = Sound::square(s.phase, 0.5) * 0.2;
    }

    s.phase += add;
    if (s.phase > 2.0 * PI) {
      s.phase -= 2.0 * PI;
    }
  }

  s.idx++;
  s.idx = s.idx % indexBar;
  return result;
}

float Looper::update(float input) {
  float result = input;
  bool useStart = false;

  const int sr = Config::instance().getSampleRate();
  int slackInterval = static_cast<int>(static_cast<float>(sr) * 0.5f);

  int indexSoundStart = sr * 60 / static_cast<int>(bpm_);
  int indexBar = indexSoundStart * (numBars_ * 4);

  for (size_t t = 0; t < tracks_.size(); t++) {
    Track &track = tracks_[t];
    if (idx_ < static_cast<int>(track.data.size())) {
      result += track.data[idx_];
      if (static_cast<int>(t) == activeTrack_ && recording_) {
        if (input == 0 && tracks_[activeTrack_].noInputYet) {
          tracks_[activeTrack_].recordingStartIdx = idx_;
        }
        // if (input == 0 && abs(idx_ - tracks_[activeTrack_].recordingStartIdx)
        // <
        //                       slackInterval) {
        //  Let this be, so that we don't overwrite here.
        //} else {
        track.data[idx_] += input;
        tracks_[activeTrack_].noInputYet = false;
        //}
        // TODO: Fix this smooth transition thing
      }
    }
  }

  idx_++;
  idx_ = idx_ % indexBar;

  if (metronomeEnabled_) {
    result += generateMetronomeSample() * metronomeVolume_;
  }

  return result;
}

bool Looper::setMetronomeSampler(const std::string &waveFileHigh,
                                 const std::string &waveFileLow) {
  namespace fs = std::filesystem;

  if (!fs::exists(waveFileHigh) || !fs::is_regular_file(waveFileHigh)) {
    std::cerr << "Error: High metronome sample not found: " << waveFileHigh
              << std::endl;
    return false;
  }

  if (!fs::exists(waveFileLow) || !fs::is_regular_file(waveFileLow)) {
    std::cerr << "Error: Low metronome sample not found: " << waveFileLow
              << std::endl;
    return false;
  }

  int chanHigh = 0, samplerateHigh = 0, bpsHigh = 0, sizeHigh = 0;
  int chanLow = 0, samplerateLow = 0, bpsLow = 0, sizeLow = 0;

  char *dataHigh =
      loadWAV(waveFileHigh, chanHigh, samplerateHigh, bpsHigh, sizeHigh);
  char *dataLow = loadWAV(waveFileLow, chanLow, samplerateLow, bpsLow, sizeLow);

  if (!dataHigh || !dataLow) {
    std::cerr << "Error: Failed to load one or both metronome samples."
              << std::endl;
    return false;
  }

  int expectedRate = Config::instance().getSampleRate();
  if (samplerateHigh != expectedRate || samplerateLow != expectedRate) {
    std::cerr << "Error: Metronome sample rate mismatch. Expected "
              << expectedRate << " Hz, got " << samplerateHigh << " / "
              << samplerateLow << " Hz." << std::endl;

    delete[] dataHigh;
    delete[] dataLow;
    return false;
  }

  int numSamplesHigh = sizeHigh / (bpsHigh / 8);
  int numSamplesLow = sizeLow / (bpsLow / 8);

  std::vector<short> rawHigh = convertToVector(dataHigh, numSamplesHigh);
  std::vector<short> rawLow = convertToVector(dataLow, numSamplesLow);

  delete[] dataHigh;
  delete[] dataLow;

  // If there are 2 channels, take only the left (even indices)
  auto extractMono = [](const std::vector<short> &input,
                        int channels) -> std::vector<short> {
    if (channels <= 1)
      return input;
    std::vector<short> mono;
    mono.reserve(input.size() / channels);
    for (size_t i = 0; i < input.size(); i += channels)
      mono.push_back(input[i]); // left channel only
    return mono;
  };

  rawHigh = extractMono(rawHigh, chanHigh);
  rawLow = extractMono(rawLow, chanLow);

  auto toFloat = [](const std::vector<short> &input) -> std::vector<float> {
    std::vector<float> out;
    out.reserve(input.size());
    constexpr float invMax = 1.0f / 32768.0f;
    for (short s : input)
      out.push_back(static_cast<float>(s) * invMax);
    return out;
  };

  metronomeSamplesHigh_ = toFloat(rawHigh);
  metronomeSamplesLow_ = toFloat(rawLow);
  metronomeUseSampler_ = true;

  return true;
}
