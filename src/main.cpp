#include "MidiFile.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <ncurses.h>
#include <optional>
#include <thread>
#include <vector>

#include <stdio.h>

#include "adsr.hpp"
#include "effect.hpp"
#include "keyboard.hpp"

#define SAMPLE_RATE 44100

// MIDI note number to frequency
float noteToFreq(int note) { return 440.0f * pow(2, (note - 69) / 12.0f); }

bool fileExists(const std::string &file) {
  return std::filesystem::exists(file);
}

class PlayConfig {
public:
  ADSR adsr;
  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Sound::Rank<short>::Preset rankPreset = Sound::Rank<short>::Preset::None;
  std::string waveFile;
  std::string midiFile;
  std::optional<Effect<short>> effectFIR = std::nullopt;
  std::optional<Effect<short>> effectChorus = std::nullopt;
  std::optional<Effect<short>> effectIIR = std::nullopt;
  std::optional<Effect<short>> effectVibrato = std::nullopt;
  std::optional<Effect<short>> effectTremolo = std::nullopt;
  float volume = 1.0;
  float duration = 0.1f;
  int parallelization = 8; // Number of threads to use in keyboard preparation

  void printConfig() {
    start_color(); // Enable color functionality

    // Define color pairs
    init_pair(2, COLOR_GREEN, COLOR_BLACK);  // Green for visualization
    init_pair(4, COLOR_WHITE, COLOR_BLACK);  // White Bold (Section Titles)
    init_pair(5, COLOR_YELLOW, COLOR_BLACK); // Orange/Yellow (Values)

    // Print configuration details
    attron(A_BOLD | COLOR_PAIR(4));
    printw("Keyboard sound configuration:\n");
    attroff(A_BOLD | COLOR_PAIR(4));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  Volume: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%.2f\n", volume);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  Notes-wave-map: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%s\n", waveFile.size() > 0 ? waveFile.c_str() : "none");
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  Waveform: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%s\n", rankPreset != Sound::Rank<short>::Preset::None
                       ? Sound::Rank<short>::presetStr(rankPreset).c_str()
                       : Sound::typeOfWave(waveForm).c_str());
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  ADSR:\n");
    attroff(A_BOLD | COLOR_PAIR(4));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    Amplitude: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d\n", adsr.amplitude);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    Quantas: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d\n", adsr.quantas);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    QADSR: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d %d %d %d\n", adsr.qadsr[0], adsr.qadsr[1], adsr.qadsr[2],
           adsr.qadsr[3]);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    Length: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d\n", adsr.length);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    Quantas_length: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d\n", adsr.quantas_length);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    Sustain_level: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d\n", adsr.sustain_level);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("    Visualization: [see below]\n");
    attroff(A_BOLD | COLOR_PAIR(4));

    // Print the cool ASCII visualization in **green**
    attron(COLOR_PAIR(2) | A_BOLD);
    printw("%s", adsr.getCoolASCIVisualization("    ").c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);

    if (effectFIR) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  FIRs: ");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%lu\n", effectFIR->firs.size());
      attroff(COLOR_PAIR(5));
      for (size_t i = 0; i < effectFIR->firs.size(); i++) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] IR length: ", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        printw("%zu, Normalized: %s\n", effectFIR->firs[i].getIRLen(),
               effectFIR->firs[i].getNormalization() ? "true" : "false");
        attroff(COLOR_PAIR(5));
      }
    }

    if (effectIIR) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  IIRs: ");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%lu\n", effectIIR->iirs.size());
      attroff(COLOR_PAIR(5));
      for (size_t i = 0; i < effectIIR->iirs.size(); i++) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] Memory: ", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        printw("%u\n", effectIIR->iirs[i].memory);
        attroff(COLOR_PAIR(5));
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] poles:", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        for (int a = 0; a < effectIIR->iirs[i].as.size(); a++) {
          printw(" %f", effectIIR->iirs[i].as[a]);
        }
        printw("\n");
        attroff(COLOR_PAIR(5));
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] zeroes:", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        for (int b = 0; b < effectIIR->iirs[i].bs.size(); b++) {
          printw(" %f", effectIIR->iirs[i].bs[b]);
        }
        printw("\n");
        attroff(COLOR_PAIR(5));
      }
    }

    /* ------- Chorus
     * -----------------------------------------------------------*/
    if (effectChorus) {
      if (const auto *c =
              std::get_if<Effect<short>::ChorusConfig>(&effectChorus->config)) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("  Chorus: delay=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", c->delay);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("depth=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", c->depth);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("voices=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%d\n", c->numVoices);
        attroff(COLOR_PAIR(5));
      }
    } else {
      printw("  No chorus\n");
    }

    /* ------- Vibrato
     * ----------------------------------------------------------*/
    if (effectVibrato) {
      if (const auto *v = std::get_if<Effect<short>::VibratoConfig>(
              &effectVibrato->config)) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("  Vibrato: frequency=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", v->frequency);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("depth=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f\n", v->depth);
        attroff(COLOR_PAIR(5));
      }
    } else {
      printw("  No vibrato\n");
    }

    /* ------- Tremolo
     * ----------------------------------------------------------*/
    if (effectTremolo) {
      if (const auto *t = std::get_if<Effect<short>::TremoloConfig>(
              &effectTremolo->config)) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("  Tremolo: frequency=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", t->frequency);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("depth=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f\n", t->depth);
        attroff(COLOR_PAIR(5));
      }
    } else {
      printw("  No tremolo\n");
    }

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  note length: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%.2f s\n", duration * adsr.quantas);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  A4 frequency: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%.2f Hz\n", notes::getFrequency("A4"));
    attroff(COLOR_PAIR(5));

    refresh(); // Refresh the screen to apply changes
  }
};

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf("   --form: form of sound [sine (default), triangular, saw, supersaw, "
         "\n");
  printf("           square, fattriangle, pulsesquare, sinesawdrone, "
         "supersawsub, \n");
  printf(
      "           glitchmix, lushpad, retroLead, bassgrowl, ambientdrone,\n");
  printf("           synthstab, glassbells, organtone]\n");
  printf("   -e|--echo: Add an echo effect\n");
  printf("   --chorus: Add a chorus effect with default settings\n");
  printf("   --chorus_delay [float]: Set the chorus delay factor, default: "
         "0.45\n");
  printf("   --chorus_depth [float]: Set the chorus depth factor, in pitch "
         "cents, default: "
         "3\n");
  printf("   --chorus_voices[int]: Set the chorus voices, default: 3\n");
  printf("   --vibrato: Add a vibrato effect with default settings\n");
  printf("   --vibrato_depth [float]: Set the vibrato depth factor, default: "
         "0.3\n");
  printf("   --vibrato_frequency [float]: Set the vibrato frequency, in Hertz "
         " default: 6\n");
  printf("   --tremolo: Add a tremolo effect with default settings\n");
  printf("   --tremolo_depth [float]: Set the tremolo depth factor [0-1], "
         "default: "
         "1.0\n");
  printf("   --tremolo_frequency [float]: Set the tremolo frequency, in Hertz "
         " default: 18\n");
  printf("   -r|--reverb [file]: Add a reverb effect based on IR response in "
         "this wav file\n");
  printf("   --notes [file]: Map notes to .wav files as mapped in this .json "
         "file\n");
  printf("   --midi [file]: Play this MIDI (.mid) file\n");
  printf("   --volume [float]: Set the volume knob (default 1.0)\n");
  printf("   --duration [float]: Note ADSR quanta duration in seconds (default "
         "0.1)\n");
  printf("   --adsr [int,int,int,int]: Set the ADSR quant intervals "
         "comma-separated (default 1,1,3,3)\n");
  printf("   --sustain [float]: Set the sustain level [0,1] (default 0.8)\n");
  printf(
      "   --lowpass [float]: Set the lowpass filter cut off frequency in Hz\n");
  printf("                   (default no low pass)\n");
  printf("   --highpass [float]: Set the highpass filter cut off frequency in "
         "Hz\n");
  printf("                (default no highpass)\n");
  printf("   --parallelization [int]: Number of threads used in keyboard "
         "preparation default: 8");
  printf("\n");
  printf("%s compiled %s %s\n", argv0, __DATE__, __TIME__);
}

void loaderFunc(unsigned ticks, unsigned tick) {
  printf("\rLoading %d %%", tick * 100 / ticks);
  if (tick == ticks) {
    printf("\r");
  }
  fflush(stdout);
}

int parseArguments(int argc, char *argv[], PlayConfig &config) {
  auto ensureVibrato = [&](float defFreq = 6.0f, float defDepth = 0.3f) {
    if (!config.effectVibrato) {
      Effect<short> e;
      e.effectType = Effect<short>::Type::Vibrato;
      e.config = Effect<short>::VibratoConfig{defFreq, defDepth};
      e.sampleRate = SAMPLERATE; // make sure SR is set once
      config.effectVibrato = e;
    }
  };
  auto ensureChorus = [&](float defDelay = 0.05f, float defDepth = 3.0f,
                          int defVoices = 3) {
    /* Create the effect only if it does not exist or is of another type */
    if (!config.effectChorus) {
      Effect<short> e;
      e.effectType = Effect<short>::Type::Chorus;
      e.config = Effect<short>::ChorusConfig{defDelay, defDepth, defVoices};
      e.sampleRate = SAMPLERATE; // make sure SR is set once
      config.effectChorus = e;
    }
  };
  auto ensureTremolo = [&](float defFreq = 5.0f, float defDepth = 0.5f) {
    if (!config.effectTremolo) {
      Effect<short> e;
      e.effectType = Effect<short>::Type::Tremolo;
      e.config = Effect<short>::TremoloConfig{defFreq, defDepth};
      e.sampleRate = SAMPLERATE; // set once
      config.effectTremolo = e;
    }
  };

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--form") {
      if (i + 1 < argc) {
        std::string form = argv[i + 1];
        if (form == "triangular") {
          config.rankPreset = Sound::Rank<short>::Preset::Triangular;
        } else if (form == "saw") {
          config.rankPreset = Sound::Rank<short>::Preset::Saw;
        } else if (form == "square") {
          config.rankPreset = Sound::Rank<short>::Preset::Square;
        } else if (form == "sine") {
          config.rankPreset = Sound::Rank<short>::Preset::Sine;
        } else if (form == "supersaw") {
          config.rankPreset = Sound::Rank<short>::Preset::SuperSaw;
        } else if (form == "fattriangle") {
          config.rankPreset = Sound::Rank<short>::Preset::FatTriangle;
        } else if (form == "pulsesquare") {
          config.rankPreset = Sound::Rank<short>::Preset::PulseSquare;
        } else if (form == "sinesawdrone") {
          config.rankPreset = Sound::Rank<short>::Preset::SineSawDrone;
        } else if (form == "supersawsub") {
          config.rankPreset = Sound::Rank<short>::Preset::SuperSawWithSub;
        } else if (form == "glitchmix") {
          config.rankPreset = Sound::Rank<short>::Preset::GlitchMix;
        } else if (form == "lushpad") {
          config.rankPreset = Sound::Rank<short>::Preset::LushPad;
        } else if (form == "retrolead") {
          config.rankPreset = Sound::Rank<short>::Preset::RetroLead;
        } else if (form == "bassgrowl") {
          config.rankPreset = Sound::Rank<short>::Preset::BassGrowl;
        } else if (form == "ambientdrone") {
          config.rankPreset = Sound::Rank<short>::Preset::AmbientDrone;
        } else if (form == "synthstab") {
          config.rankPreset = Sound::Rank<short>::Preset::SynthStab;
        } else if (form == "glassbells") {
          config.rankPreset = Sound::Rank<short>::Preset::GlassBells;
        } else if (form == "organtone") {
          config.rankPreset = Sound::Rank<short>::Preset::OrganTone;
        }
      }
    } else if (arg == "--notes" && i + 1 < argc) {
      config.waveFile = argv[i + 1];
    } else if (arg == "--adsr" && (i + 1 < argc)) {
      std::string adsrArg = argv[i + 1];
      std::stringstream ss(adsrArg);
      std::string number;
      int idx = 0;

      while (std::getline(ss, number, ',') && idx < 4) {
        config.adsr.qadsr[idx++] = std::stoi(number);
      }

      if (idx != 4) {
        printf("Error: Expected 4 ADSR parameters separated by commas.\n");
        printHelp(argv[0]);
        return -1;
        return 1;
      }

      config.adsr.update_len();
      ++i; // skip the next argument as it's already processed
    } else if (arg == "-e" || arg == "--echo") {
      FIR fir(SAMPLERATE);
      fir.setResonance({1.0, 0.5, 0.25, 0.125, 0.0515, 0.02575}, 1.0);
      Effect<short> effect;
      effect.sampleRate = SAMPLERATE;
      effect.effectType = Effect<short>::Type::Fir;
      config.effectFIR = effect;
      config.effectFIR->addFIR(fir);

    } else if (arg == "--vibrato") { // enable with defaults
      ensureVibrato();               // only creates if missing

    } else if (arg == "--vibrato_depth" && i + 1 < argc) {
      ensureVibrato(); // keep existing freq
      auto &v =
          std::get<Effect<short>::VibratoConfig>(config.effectVibrato->config);
      v.depth = std::stof(argv[i + 1]); // change depth only

    } else if (arg == "--vibrato_frequency" && i + 1 < argc) {
      ensureVibrato(); // keep existing depth
      auto &v =
          std::get<Effect<short>::VibratoConfig>(config.effectVibrato->config);
      v.frequency = std::stof(argv[i + 1]); // change frequency only
    } else if (arg == "--tremolo") {        // enable with defaults
      ensureTremolo();

    } else if (arg == "--tremolo_depth" && i + 1 < argc) {
      ensureTremolo(); // keep existing freq
      auto &v =
          std::get<Effect<short>::TremoloConfig>(config.effectVibrato->config);
      v.depth = std::stof(argv[i + 1]);
    } else if (arg == "--tremolo_frequency" && i + 1 < argc) {
      ensureTremolo(); // keep existing depth
      auto &v =
          std::get<Effect<short>::TremoloConfig>(config.effectVibrato->config);
      v.frequency = std::stof(argv[i + 1]);
    } else if (arg == "--lowpass" && i + 1 < argc) {
      Effect<short> effect;
      effect.effectType = Effect<short>::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = SAMPLERATE;
      IIR<short> lowPass = IIRFilters::lowPass<short>(
          config.effectIIR->sampleRate, std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--highpass" && i + 1 < argc) {
      Effect<short> effect;
      effect.effectType = Effect<short>::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = SAMPLERATE;
      IIR<short> lowPass = IIRFilters::highPass<short>(
          config.effectIIR->sampleRate, std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--chorus") { // enable with defaults
      ensureChorus();
    } else if (arg == "--chorus_delay" && i + 1 < argc) {
      ensureChorus();
      auto &v =
          std::get<Effect<short>::ChorusConfig>(config.effectVibrato->config);
      v.delay = std::stof(argv[i + 1]);
    } else if (arg == "--chorus_depth" && i + 1 < argc) {
      ensureChorus();
      auto &v =
          std::get<Effect<short>::ChorusConfig>(config.effectVibrato->config);
      v.depth = std::stof(argv[i + 1]);
    } else if (arg == "--chorus_voices" && i + 1 < argc) {
      ensureChorus();
      auto &v =
          std::get<Effect<short>::ChorusConfig>(config.effectVibrato->config);
      v.numVoices = std::atoi(argv[i + 1]);
    } else if (arg == "--parallelization" && i + 1 < argc) {
      config.parallelization = std::atoi(argv[i + 1]);
    } else if (arg == "--midi" && i + 1 < argc) {
      config.midiFile = argv[i + 1];
    } else if (arg == "-r" || arg == "--reverb" && i + 1 < argc) {
      FIR fir(SAMPLERATE);
      fir.loadFromFile(argv[i + 1]);
      fir.setNormalization(true);
      Effect<short> effect;
      effect.sampleRate = SAMPLERATE;
      effect.effectType = Effect<short>::Type::Fir;
      config.effectFIR = effect;
      config.effectFIR->addFIR(fir);
    } else if (arg == "--sustain" && i + 1 < argc) {
      config.adsr.sustain_level =
          std::stof(argv[i + 1]) * config.adsr.amplitude;
    } else if (arg == "--volume" && i + 1 < argc) {
      config.volume = std::stof(argv[i + 1]);
    } else if (arg == "--duration" && i + 1 < argc) {
      config.duration = std::stof(argv[i + 1]);
      config.adsr.setLength(
          static_cast<int>(SAMPLERATE * config.duration * config.adsr.quantas));
    } else if (arg == "-h" || arg == "--help") {
      printHelp(argv[0]);
      return -1;
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  float duration = 0.1f;
  short amplitude = 32767;
  int maxPolyphony = 50;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(SAMPLERATE * duration));
  Keyboard keyboard(maxPolyphony);
  int rankIndex = 0;
  std::vector<Sound::Rank<short>::Preset> presets = {
      Sound::Rank<short>::Preset::Sine,
      Sound::Rank<short>::Preset::Saw,
      Sound::Rank<short>::Preset::Square,
      Sound::Rank<short>::Preset::Triangular,
      Sound::Rank<short>::Preset::SuperSaw,
      Sound::Rank<short>::Preset::FatTriangle,
      Sound::Rank<short>::Preset::PulseSquare,
      Sound::Rank<short>::Preset::SineSawDrone,
      Sound::Rank<short>::Preset::SuperSawWithSub,
      Sound::Rank<short>::Preset::GlitchMix,
      Sound::Rank<short>::Preset::OrganTone,
      Sound::Rank<short>::Preset::LushPad,
      Sound::Rank<short>::Preset::RetroLead,
      Sound::Rank<short>::Preset::BassGrowl,
      Sound::Rank<short>::Preset::AmbientDrone,
      Sound::Rank<short>::Preset::SynthStab,
      Sound::Rank<short>::Preset::GlassBells};

  PlayConfig config;
  config.adsr = adsr;
  config.rankPreset = presets[rankIndex];
  int c = parseArguments(argc, argv, config);
  if (c < 0) {
    return 0;
  } else if (c > 0) {
    return 1;
  }

  printf("Processing buffers... preparing sound..\n");

  if (config.waveFile.size() > 0) {
    keyboard.loadSoundMap(config.waveFile);
    config.waveForm = Sound::WaveForm::WaveFile;
  }
  keyboard.setLoaderFunc(loaderFunc);
  keyboard.setVolume(config.volume);
  std::vector<Effect<short>> effects;
  if (config.effectFIR) {
    effects.push_back(*config.effectFIR);
  }
  if (config.effectChorus) {
    effects.push_back(*config.effectChorus);
  }
  if (config.effectIIR) {
    effects.push_back(*config.effectIIR);
  }
  if (config.effectVibrato) {
    effects.push_back(*config.effectVibrato);
  }
  if (config.effectTremolo) {
    effects.push_back(*config.effectTremolo);
  }
  auto start = std::chrono::high_resolution_clock::now();
  keyboard.prepareSound(SAMPLERATE, config.adsr, config.rankPreset, effects,
                        config.parallelization);
  auto end = std::chrono::high_resolution_clock::now();
  auto prepTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();
  printf("Keyboard preparation time: %0.4f seconds",
         static_cast<double>(prepTime) / 1000.0);
  printf("\nSound OK!\n");
  initscr();            // Initialize the library
  cbreak();             // Line buffering disabled
  keypad(stdscr, TRUE); // Enable special keys
  noecho();             // Don't show the key being pressed
  scrollok(stdscr, TRUE);

  if (config.midiFile.size() > 0) {

    config.printConfig();

    if (!fileExists(config.midiFile)) {
      printf("error: MIDI file provided, '%s', does not seem to exist?\nPlease "
             "check, maybe there was a spelling error?\n",
             config.midiFile.c_str());
      return 1;
    }

    smf::MidiFile midiFile;
    midiFile.read(config.midiFile);
    midiFile.doTimeAnalysis();
    midiFile.linkNotePairs();

    double totalDuration = midiFile.getFileDurationInSeconds();
    int totalSamples = SAMPLE_RATE * totalDuration;
    std::map<int, std::vector<std::string>> notesMap;

    for (int track = 0; track < midiFile.getTrackCount(); track++) {
      for (int event = 0; event < midiFile[track].size(); event++) {
        if (midiFile[track][event].isNoteOn()) {
          int note = midiFile[track][event][1];
          float frequency = noteToFreq(note);

          std::string noteKey = notes::getClosestNote(frequency);

          float duration = midiFile[track][event].getDurationInSeconds();
          float startTime = midiFile[track][event].seconds;

          int startSample = SAMPLE_RATE * startTime;

          notesMap[startSample].push_back(noteKey);
        }
      }
    }

    printw("Playing the midi file %s\n", config.midiFile.c_str());

    int playPin = 0;
    for (const auto &entry : notesMap) {
      int sampleTime = entry.first;
      std::this_thread::sleep_for(
          std::chrono::microseconds((sampleTime - playPin) * 23));
      playPin = sampleTime;
      const auto &notes = notesMap[sampleTime];
      for (auto &note : notes) {
        keyboard.registerNote(note);
      }
    }

  } else {

    config.printConfig();
    keyboard.printInstructions();

    while (true) {
      int ch = getch();
      keyboard.registerButtonPress(ch);
      if (ch == 'o' || ch == 'p') {
        clear();
        config.printConfig();
        keyboard.printInstructions();
      } else if (ch == 'O' || ch == 'P') {
        keyboard.teardown();
        printw("Updating the keyboard..\n");
        keyboard.setup(maxPolyphony);
        if (ch == 'P') {
          rankIndex = (rankIndex + 1) % presets.size();
        } else {
          rankIndex = (rankIndex + presets.size() - 1) % presets.size();
        }
        keyboard.prepareSound(SAMPLERATE, config.adsr, presets[rankIndex],
                              effects, config.parallelization);
        clear();
        config.rankPreset = presets[rankIndex];
        config.printConfig();
        keyboard.printInstructions();
        printw("Updated to new preset %s\n",
               Sound::Rank<short>::presetStr(presets[rankIndex]).c_str());
      } else if (ch == 'W' || ch == 'E') {
        keyboard.setVolume(keyboard.getVolume() - (ch == 'E' ? -0.1 : 0.1));
        keyboard.teardown();
        keyboard.setup(maxPolyphony);
        keyboard.prepareSound(SAMPLERATE, config.adsr, presets[rankIndex],
                              effects, config.parallelization);
        clear();
        config.printConfig();
        keyboard.printInstructions();
      }
      printw("%c\n", c);
    }
  }

  endwin(); // End curses mode

  return 0;
}
