#include "looper.hpp"
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
    : tracks_(NUM_TRACKS), activeTrack_(0), recording_(false), numBars_(8),
      bpm_(120.0f), metronomeEnabled_(false), metronomePhase_(0.0),
      metronomeIncrement_(0.0), metronomeVolume_(0.25f) {
  updateLoopLength();
  updateMetronomeIncrement();

  // Allocate per-track buffers to loop length
  for (auto &t : tracks_) {
    t.data.assign(loopLengthSamples_, 0.0f);
    t.writePos = 0;
    t.armed = false;
  }
}

// --- Recording control ---
void Looper::setRecording(bool enabled) { recording_ = enabled; }

bool Looper::isRecording() const { return recording_; }

void Looper::setActiveTrack(int index) {
  std::scoped_lock lock(mtx_);
  if (index < 0)
    index = 0;
  if (index >= NUM_TRACKS)
    index = NUM_TRACKS - 1;
  activeTrack_ = index;
}

int Looper::getActiveTrack() const { return activeTrack_; }

void Looper::clearTrack(int index) {
  if (index < 0 || index >= NUM_TRACKS)
    return;
  std::scoped_lock lock(mtx_);
  auto &t = tracks_[index];
  t.data.assign(loopLengthSamples_, 0.0f);
  t.writePos = 0;
}

// --- Loop length control ---
void Looper::setNumBars(int bars) {
  if (bars < 1)
    bars = 1;
  std::scoped_lock lock(mtx_);
  if (numBars_ == bars)
    return;
  numBars_ = bars;
  updateLoopLength();

  // Reallocate tracks to the new loop length (clear for simplicity)
  for (auto &t : tracks_) {
    t.data.assign(loopLengthSamples_, 0.0f);
    t.writePos = 0;
  }
}

int Looper::getNumBars() const { return numBars_; }

// --- Metronome control ---
void Looper::setBPM(float bpm) {
  if (bpm < 0.0f)
    bpm = 0.0f;
  std::scoped_lock lock(mtx_);
  if (bpm_ == bpm)
    return;
  bpm_ = bpm;
  updateMetronomeIncrement();
  updateLoopLength();

  for (auto &t : tracks_) {
    t.writePos = t.writePos % std::max<std::size_t>(1, loopLengthSamples_);
  }
}

float Looper::getBPM() const { return bpm_; }

void Looper::enableMetronome(bool enable) {
  std::scoped_lock lock(mtx_);
  metronomeEnabled_ = enable;
}

bool Looper::isMetronomeEnabled() const { return metronomeEnabled_; }

// --- Audio processing (add-only, no copies) ---
void Looper::fillBuffer(float *buffer, const int len) {
  /*
  if (!buffer || len <= 0)
    return;

  // Snapshot small control state quickly (no heavy work under lock)
  bool metroEnabledLocal;
  std::size_t loopLenLocal;
  int activeTrackLocal;

  {
    std::scoped_lock lock(mtx_);
    metroEnabledLocal = metronomeEnabled_;
    loopLenLocal = loopLengthSamples_;
    activeTrackLocal = activeTrack_;
  }

  if (loopLenLocal == 0)
    return;

  // Use track 0 as the shared transport position (single modulo per sample)
  std::size_t pos = tracks_[0].writePos;
  const bool doRecord = recording_.load(std::memory_order_relaxed);

  // Hot loop
  for (int i = 0; i < len; ++i) {
    buffer[i] = 0;
    // 1) Read current synth sample BEFORE we add anything
    const float in = buffer[i];

    // 2) Optionally record this input into the active track at current pos
    if (doRecord) {
      auto &trk = tracks_[activeTrackLocal];
      // Replace mode (use += for overdub)
      trk.data[pos] = in;
    }

    // 3) Mix playback from all tracks at current pos (ADD-ONLY)
    float acc = 0.0f;
    // Unroll small loop for 4 tracks (tiny speed win)
    acc += tracks_[0].data[pos];
    acc += tracks_[1].data[pos];
    acc += tracks_[2].data[pos];
    acc += tracks_[3].data[pos];

    // 4) Add metronome tick (if enabled)
    float acc = 0;
    if (metroEnabledLocal) {
      acc += generateMetronomeSample() * metronomeVolume_;
    }

    // 5) Write back (ADD ONLY)
    buffer[i] += acc;

    // 6) Advance shared position
    pos = wrap_advance(pos, loopLenLocal);
  }

  // Keep all track cursors in sync with the shared transport
  // (tiny cost, outside hot loop)
  for (auto &t : tracks_) {
    t.writePos = pos;
  }
  */
}

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
  const double totalSeconds = static_cast<double>(numBars_) *
                              static_cast<double>(BEATS_PER_BAR) *
                              secondsPerBeat;
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
  struct LooperEnv {
    int idx = 0;
  };
  static LooperEnv s;

  float result = input;
  bool useStart = false;

  const int sr = Config::instance().getSampleRate();

  int indexSoundStart = sr * 60 / static_cast<int>(bpm_);
  int indexBar = indexSoundStart * numBars_;

  for (size_t t = 0; t < tracks_.size(); t++) {
    Track &track = tracks_[t];
    if (s.idx < static_cast<int>(track.data.size())) {
      result += track.data[s.idx];
      if (static_cast<int>(t) == activeTrack_ && recording_) {
        track.data[s.idx] = input;
      }
    }
  }

  s.idx++;
  s.idx = s.idx % indexBar;

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
