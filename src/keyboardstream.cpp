#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <ncurses.h>
#include <thread>
#include <vector>

#include "effect.hpp"
#include "fir.hpp"
#include "keyboardstream.hpp"
#include "notes.hpp"
#include "sound.hpp"
#include "waveread.hpp"

void KeyboardStream::printInstructions() {
  start_color();

  // Define colors
  init_pair(4, COLOR_WHITE, COLOR_BLACK);  // Labels (White Bold)
  init_pair(5, COLOR_YELLOW, COLOR_BLACK); // Values (Orange/Yellow)
  init_pair(6, COLOR_BLACK, COLOR_WHITE);  // Key characters
  init_pair(7, COLOR_BLUE, COLOR_WHITE);   // Note names

  // Section header
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("These keys are available on your keyboard:\n");
  attroff(COLOR_PAIR(4) | A_BOLD);

  std::vector<int> rowNumber = {'1', '2', '3', '4', '5',
                                '6', '7', '8', '9', '0'};
  std::vector<int> rowQwert = {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i'};
  std::vector<int> rowHome = {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l'};
  std::vector<int> rowBottom = {'z', 'x', 'c', 'v', 'b', 'n', 'm', ','};

  // Helper lambda to print a row.
  auto printRow = [this](const std::vector<int> &row, const char *prefix) {
    attron(COLOR_PAIR(4) | A_BOLD);
    printw("%s| ", prefix);

    attroff(COLOR_PAIR(4) | A_BOLD);
    for (int key : row) {
      auto it = this->keyPressToNote.find(key);
      if (it != this->keyPressToNote.end()) {
        attron(COLOR_PAIR(6));
        printw("%c ", key);
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(7));
        printw("[%s]", it->second.c_str());
        attroff(COLOR_PAIR(7));
        attron(COLOR_PAIR(4) | A_BOLD);
        printw(" | ");
        attroff(COLOR_PAIR(4) | A_BOLD);
      }
    }
    printw("\n");
  };

  // Print each row in order.
  printRow(rowNumber, "");
  printRow(rowQwert, "  ");
  printRow(rowHome, "    ");
  printRow(rowBottom, "      ");

  // Volume knob display
  attron(COLOR_PAIR(4) | A_BOLD);
  printw("\nVolume knob set to: ");
  attroff(COLOR_PAIR(4) | A_BOLD);
  attron(COLOR_PAIR(5));
  printw("%.2f\n", this->volume);
  attroff(COLOR_PAIR(5));

  attron(COLOR_PAIR(4) | A_BOLD);
  printw("Press 'p'/'o' to +/- one octave\n");
  printw("Press 'P'/'O' to +/- keyboard preset sounds\n");
  printw("Press SPACE to toggle recording. Recording active: %s\n", (this->looper.isRecording() ? "Yes" : "No"));
  printw("Press '.' to toggle metronome. Metronome active: %s\n", (this->looper.isMetronomeEnabled() ? "Yes" : "No"));
  attroff(COLOR_PAIR(4) | A_BOLD);
}

void KeyboardStream::prepareSound(int sampleRate, ADSR &adsr,
                                  std::vector<Effect<float>> &effects) {
  this->adsr = adsr;
  this->sampleRate = sampleRate;
  {
    // First entry of the effects vector is dedicated to IIR high-pass and
    // low-pass filters: effects[0][0] -> High-pass filter effects[0][1] ->
    // Low-pass filter
    Effect<float> effect;
    effect.effectType = Effect<float>::Type::Iir;
    effect.sampleRate = sampleRate;
    IIR<float> highPass = IIRFilters::highPass<float>(sampleRate, 0.0);
    IIR<float> lowPass =
        IIRFilters::lowPass<float>(sampleRate, sampleRate * 0.5);
    effect.iirs.push_back(highPass);
    effect.iirs.push_back(lowPass);
    this->effects.push_back(effect);
  }
  this->effects.insert(this->effects.end(), effects.begin(), effects.end());

  if (!this->soundMap.empty()) {
    this->synth.emplace_back(SAMPLERATE, this->tuning);
    this->synth[0].setVolume(0.5);
    this->synth[0].setSoundMap(this->soundMap);
    this->synth[0].setEffects(this->effects);
  } else {
    this->setupStandardSynthConfig();
  }
}

void KeyboardStream::setupStandardSynthConfig() {
  this->synth.reserve(4);
  for (int i = 0; i < 4; i++) {
    this->synth.emplace_back(SAMPLERATE, this->tuning);
  }
  for (int i = 0; i < 4; i++) {
    this->synth[i].setVolume(i == 0 ? 0.5 : 0);
    this->synth[i].setEffects(this->effects);
    this->synth[i].setLegato(this->legatoMode, this->legatoSpeed);
  }
}

void KeyboardStream::registerNote(const std::string &note) {
  // Always create a fresh note entry
  NotePress np;
  np.time = KeyboardStream::currentTimeMillis();
  np.note = note;
  np.adsr = this->adsr;
  np.frequency = notes::getFrequency(note, this->tuning);
  np.index = 0;
  np.release = false;

  if (this->legatoMode) {
    std::string legatoNote = "C4";
    auto it = this->notesPressed.find(legatoNote);

    if (it == this->notesPressed.end()) {
      this->resetLegato();
      this->notesPressed[legatoNote] = np;
    } else {
      it->second.frequency = np.frequency;
      if (it->second.note == note) {
        it->second.index = 0; // this->adsr.get_decay_start_index();
      }
      it->second.note = note;
    }
  } else {
    auto it = this->notesPressed.find(note);

    if (it != this->notesPressed.end()) {
      // Note already exists – create a new entry with release = true
      NotePress releasedNote;
      releasedNote = it->second;
      releasedNote.release = true;
      releasedNote.phase = 0;
      releasedNote.time = KeyboardStream::currentTimeMillis();
      std::string newKey = note + "--" + std::to_string(releasedNote.time);
      this->notesPressed[newKey] = releasedNote;
    }

    this->notesPressed[note] = np;
  }
}

void KeyboardStream::registerNoteRelease(const std::string &note) {
  if (this->legatoMode) {
    for (auto it = notesPressed.begin(); it != notesPressed.end(); it++) {
      it->second.release = true;
    }
  } else {
    auto it = this->notesPressed.find(note);
    if (it != this->notesPressed.end()) {
      it->second.release = true;
    }
  }
}

void KeyboardStream::registerButtonPress(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    registerNote(note);
  }
  if (this->keyPressToAction.find(pressed) != this->keyPressToAction.end()) {
    this->keyPressToAction[pressed]();
  }
}

void KeyboardStream::registerButtonRelease(int pressed) {
  if (this->keyPressToNote.find(pressed) != this->keyPressToNote.end()) {
    std::string note = this->keyPressToNote[pressed];
    registerNoteRelease(note);
  }
}

void KeyboardStream::fillBuffer(float *buffer, const int len) {
  float deltaT = 1.0f / this->sampleRate;

  // auto start = std::chrono::high_resolution_clock::now();

  // if (notesPressed.size() == 0)
  //   return;

  // Add samples to YIN calc
  //this->yin.addSamples(buffer, len);

  for (int i = 0; i < len; i++) {
    float sample = 0.0f;
    buffer[i] = 0;

    for (auto it = notesPressed.begin(); it != notesPressed.end();) {
      NotePress &note = it->second;
      int index = note.index;
      float adsr;
      double freq = note.frequency;

      // Use ADSR envelope
      if (note.adsr.reached_sustain(index) && !note.release) {
        adsr = static_cast<float>(note.adsr.sustain());
      } else {
        adsr = static_cast<float>(note.adsr.response(index));
        note.index++;
      }

      if (this->legatoMode) {
        if (it->first.find("--") == std::string::npos) {
          sample += adsr * generateSample(note.note, note.phase,
                                          this->legatoRankIndex);
          this->legatoRankIndex++;
        }
      } else {
        sample += adsr * generateSample(note.note, note.phase, note.rankIndex);
        note.rankIndex++;
      }

      // Advance phase
      note.phase += 2.0f * M_PI * freq * deltaT;
      if (note.phase > 2.0f * M_PI)
        note.phase -= 2.0f * M_PI;

      // Remove if done
      if (note.index >= note.adsr.getLength()) {
        it = notesPressed.erase(it);
      } else {
        ++it;
      }
    }

    // Output sample
    float entry = sample * this->gain;
    // Apply global post effects
    entry = Sound::applyPostEffects(entry, this->effects);
    // Apply global iir filters
    for (int e = 0; e < this->effects.size(); e++) {
      for (int i = 0; i < this->effects[e].iirs.size(); i++) {
        entry = this->effects[e].iirs[i].process(entry);
      }
    }
    // Write to buffer
    buffer[i] += this->looper.update(entry);
  }

  /*
  float yinF = this->yin.getYinFrequency();
  if (yinF > 0) {
    printw("\rHearing: %s   ",
           notes::getClosestNote(yinF, notes::TuningSystem::EqualTemperament)
               .c_str());
  }*/

  // auto end = std::chrono::high_resolution_clock::now();
  // std::chrono::duration<double, std::milli> duration = end - start;

  // Optionally keep this if debugging:
  // printw(" %f (%zu) -", duration.count(), notesPressed.size());
  refresh();
}

float KeyboardStream::generateSample(std::string note, float phase, int index) {
  float result = 0;
  float min = static_cast<float>(std::numeric_limits<short>::min());
  float max = static_cast<float>(std::numeric_limits<short>::max());

  for (Oscillator &oscillator : this->synth) {
    if (oscillator.volume == 0.0)
      continue;
    result += oscillator.volume * oscillator.getSample(note, index);
  }

  result =
      std::clamp(result, static_cast<float>(std::numeric_limits<short>::min()),
                 static_cast<float>(std::numeric_limits<short>::max()));

  return result;
}

static void normalizeBuffer(std::vector<short> &buffer) {
  short maxVal = 0;
  for (short s : buffer) {
    if (std::abs(static_cast<int>(s)) > maxVal) {
      maxVal = std::abs(static_cast<int>(s));
    }
  }

  if (maxVal == 0)
    return; // avoid divide by zero

  double scale =
      static_cast<double>(std::numeric_limits<short>::max()) / maxVal;
  for (auto &s : buffer) {
    s = static_cast<short>(std::round(s * scale));
  }
}

void KeyboardStream::Oscillator::setSoundMap(
    std::map<std::string, std::string> &soundMap, bool normalize) {
  for (const auto &[key, value] : soundMap) {
    std::cout << "Key: " << key << ", Value: " << value << std::endl;
    int channels, wavSampleRate, bps, size;
    char *data;

    if ((data = loadWAV(value, channels, wavSampleRate, bps, size)) !=
        nullptr) {
      if (channels == 2) {
        std::vector<short> buffer_left_in, buffer_right_in;
        splitChannels(data, size, buffer_left_in, buffer_right_in);

        if (normalize) {
          normalizeBuffer(buffer_left_in);
          normalizeBuffer(buffer_right_in);
        }

        std::vector<short> interleaved;
        interleaved.reserve(buffer_left_in.size() * 2);
        for (size_t i = 0; i < buffer_left_in.size(); ++i) {
          interleaved.push_back(buffer_left_in[i]);
          interleaved.push_back(buffer_right_in[i]);
        }
        this->samples[key] = interleaved;
      } else {
        std::vector<short> buffer = convertToVector(data, size);
        if (normalize) {
          normalizeBuffer(buffer);
        }
        this->samples[key] = buffer;
      }
    }
  }
}

void KeyboardStream::Oscillator::setVolume(float volume) {
  this->volume = volume;
}

using Pipe = std::pair<Note, Sound::WaveForm>;
void KeyboardStream::Oscillator::setOctave(int octave) {
  this->octave = octave;
  this->updateFrequencies();
}

void KeyboardStream::Oscillator::setDetune(int detune) {
  this->detune = detune;
  this->updateFrequencies();
}

void KeyboardStream::Oscillator::setADSR(ADSR adsr) { this->adsr = adsr; }

void KeyboardStream::Oscillator::setSound(Sound::Rank<float>::Preset sound) {
  this->sound = sound;
}

float KeyboardStream::Oscillator::getSample(const std::string &note,
                                            int index) {
  // check if we are using wave samples
  if (!this->samples.empty()) {
    if (this->samples.find(note) != this->samples.end()) {
      int indexMax = static_cast<int>(this->samples[note].size());
      if (index < indexMax) {
        float maxVal16 =
            static_cast<float>(std::numeric_limits<int16_t>::max());
        return static_cast<float>(this->samples[note][index]) / maxVal16;
      }
      return 0.0;
    }
    return 0.0;
  }
  // using raw synth
  if (!this->initialized) {
    this->initialize();
  }
  float result = 0;
  std::lock_guard<std::mutex> lk(this->ranksMtx.mutex);
  if (this->legatoMode && !this->legatoRank.has_value()) {
    std::vector<Effect<float>> effectsClone(effects);
    Note n{note, this->adsr.length, this->sampleRate, this->tuning};

    float freq = notes::getFrequency(note, this->tuning);
    Sound::Rank<float> r = Sound::Rank<float>::fromPreset(
        this->sound, freq, this->adsr.length, this->sampleRate);
    r.adsr = adsr;
    for (int e = 0; e < effectsClone.size(); e++) {
      r.addEffect(effectsClone[e]);
    }

    this->legatoRank = std::move(r);
  }
  if (this->legatoRank.has_value()) {
    float frequency = notes::getFrequency(note, this->tuning);
    this->applyLegatoFrequency(frequency);
    result = this->legatoRank->generateRankSampleIndex(index);
  } else if (this->ranks.find(note) != this->ranks.end()) {
    result = this->ranks[note].generateRankSampleIndex(index);
  }
  return result;
}

void KeyboardStream::Oscillator::reset(const std::string &note) {
  std::lock_guard<std::mutex> lk(this->ranksMtx.mutex);
  if (this->ranks.find(note) != this->ranks.end()) {
    this->ranks[note].reset();
  }
}

void KeyboardStream::Oscillator::initialize() {
  auto notes = notes::getNotes(this->tuning);

  std::vector<Effect<float>> effectsClone(effects);
  for (int i = 0; i < notes.size(); ++i) {
    auto const &key = notes[i];
    Note n{key, this->adsr.length, this->sampleRate, this->tuning};

    float freq = notes::getFrequency(key, this->tuning);
    Sound::Rank<float> r = Sound::Rank<float>::fromPreset(
        this->sound, freq, this->adsr.length, this->sampleRate);
    r.adsr = adsr;
    for (int e = 0; e < effectsClone.size(); e++) {
      r.addEffect(effectsClone[e]);
    }

    { this->ranks[key] = std::move(r); }
  }

  this->legatoRank = std::nullopt;
  this->initialized = true;
}

std::string KeyboardStream::Oscillator::printSynthConfig() const {
  std::ostringstream out;

  out << "Synth Configuration:\n";
  out << "--------------------\n";
  out << "Sample Rate: " << sampleRate << "\n";
  out << "Volume: " << volume << "\n";
  out << "Octave: " << octave << "\n";
  out << "Detune: " << detune << "\n";
  out << "Sound Preset: " << Sound::Rank<float>::presetStr(sound) << "\n";

  out << "ADSR Envelope:\n";
  out << "  Amplitude: " << adsr.amplitude << "\n";
  out << "  Quantas: " << adsr.quantas << "\n";
  out << "  QADSR: [" << adsr.qadsr[0] << ", " << adsr.qadsr[1] << ", "
      << adsr.qadsr[2] << ", " << adsr.qadsr[3] << "]\n";
  out << "  Length: " << adsr.length << "\n";
  out << "  Quantas Length: " << adsr.quantas_length << "\n";
  out << "  Sustain Level: " << adsr.sustain_level << "\n";

  return out.str();
}

void KeyboardStream::printSynthConfig() const {
  int synthIndex = 1;
  for (const Oscillator &oscillator : this->synth) {
    // Section header
    attron(COLOR_PAIR(7) | A_BOLD);
    printw("======== Oscillator %d ========\n", synthIndex);
    attroff(COLOR_PAIR(7) | A_BOLD);

    attron(COLOR_PAIR(4));
    printw("%s", oscillator.printSynthConfig().c_str());
    attroff(COLOR_PAIR(4));
  }
}
