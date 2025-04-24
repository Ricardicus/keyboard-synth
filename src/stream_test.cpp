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

#include <SDL2/SDL.h>

#include "adsr.hpp"
#include "effect.hpp"
#include "keyboardstream.hpp"

constexpr int BUFFER_SIZE = 512;

// MIDI note number to frequency
float noteToFreq(int note) { return 440.0f * pow(2, (note - 69) / 12.0f); }

bool fileExists(const std::string &file) {
  return std::filesystem::exists(file);
}

void audioCallback(void *userdata, Uint8 *stream, int len) {
  auto *ks = static_cast<KeyboardStream *>(userdata);
  short *streamBuf = reinterpret_cast<short *>(stream);
  int samples = len / sizeof(short);
  ks->fillBuffer(streamBuf, samples);
}

class PlayConfig {
public:
  ADSR adsr;
  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Sound::Rank::Preset rankPreset = Sound::Rank::Preset::None;
  std::string waveFile;
  std::string midiFile;
  std::optional<Effect> effectFIR = std::nullopt;
  std::optional<Effect> effectChorus = std::nullopt;
  std::optional<Effect> effectIIR = std::nullopt;
  std::optional<Effect> effectVibrato = std::nullopt;
  std::optional<Effect> effectTremolo = std::nullopt;
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
    printw("%s\n", rankPreset != Sound::Rank::Preset::None
                       ? Sound::Rank::presetStr(rankPreset).c_str()
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

    if (effectChorus) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  Chorus: delay=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%f ", effectChorus->chorusConfig.delay);
      attroff(COLOR_PAIR(5));
      attron(A_BOLD | COLOR_PAIR(4));
      printw("depth=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%f\n", effectChorus->chorusConfig.depth);
      attroff(COLOR_PAIR(5));
      attron(A_BOLD | COLOR_PAIR(4));
      printw("voices=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%d\n", effectChorus->chorusConfig.numVoices);
      attroff(COLOR_PAIR(5));
    }

    if (effectVibrato) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  Vibrato: frequency=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%f ", effectVibrato->vibratoConfig.frequency);
      attroff(COLOR_PAIR(5));
      attron(A_BOLD | COLOR_PAIR(4));
      printw("depth=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%f\n", effectVibrato->vibratoConfig.depth);
      attroff(COLOR_PAIR(5));
    }

    if (effectTremolo) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  Tremolo: frequency=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%f ", effectTremolo->tremoloConfig.frequency);
      attroff(COLOR_PAIR(5));
      attron(A_BOLD | COLOR_PAIR(4));
      printw("depth=");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%f\n", effectTremolo->tremoloConfig.depth);
      attroff(COLOR_PAIR(5));
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
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--form") {
      if (i + 1 < argc) {
        std::string form = argv[i + 1];
        if (form == "triangular") {
          config.rankPreset = Sound::Rank::Preset::Triangular;
        } else if (form == "saw") {
          config.rankPreset = Sound::Rank::Preset::Saw;
        } else if (form == "square") {
          config.rankPreset = Sound::Rank::Preset::Square;
        } else if (form == "triangular") {
          config.rankPreset = Sound::Rank::Preset::Triangular;
        } else if (form == "sine") {
          config.rankPreset = Sound::Rank::Preset::Sine;
        } else if (form == "supersaw") {
          config.rankPreset = Sound::Rank::Preset::SuperSaw;
        } else if (form == "fattriangle") {
          config.rankPreset = Sound::Rank::Preset::FatTriangle;
        } else if (form == "pulsesquare") {
          config.rankPreset = Sound::Rank::Preset::PulseSquare;
        } else if (form == "sinesawdrone") {
          config.rankPreset = Sound::Rank::Preset::SineSawDrone;
        } else if (form == "supersawsub") {
          config.rankPreset = Sound::Rank::Preset::SuperSawWithSub;
        } else if (form == "glitchmix") {
          config.rankPreset = Sound::Rank::Preset::GlitchMix;
        } else if (form == "lushpad") {
          config.rankPreset = Sound::Rank::Preset::LushPad;
        } else if (form == "retrolead") {
          config.rankPreset = Sound::Rank::Preset::RetroLead;
        } else if (form == "bassgrowl") {
          config.rankPreset = Sound::Rank::Preset::BassGrowl;
        } else if (form == "ambientdrone") {
          config.rankPreset = Sound::Rank::Preset::AmbientDrone;
        } else if (form == "synthstab") {
          config.rankPreset = Sound::Rank::Preset::SynthStab;
        } else if (form == "glassbells") {
          config.rankPreset = Sound::Rank::Preset::GlassBells;
        } else if (form == "organtone") {
          config.rankPreset = Sound::Rank::Preset::OrganTone;
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
      Effect effect;
      effect.sampleRate = SAMPLERATE;
      effect.effectType = Effect::Type::Fir;
      config.effectFIR = effect;
      config.effectFIR->addFIR(fir);
    } else if (arg == "--chorus") {
      Effect effect;
      effect.effectType = Effect::Type::Chorus;
      config.effectChorus = effect;
      config.effectChorus->sampleRate = SAMPLERATE;
    } else if (arg == "--vibrato") {
      Effect effect;
      effect.effectType = Effect::Type::Vibrato;
      config.effectVibrato = effect;
      config.effectVibrato->sampleRate = SAMPLERATE;
    } else if (arg == "--vibrato_depth" && i + 1 < argc) {
      if (!config.effectVibrato) {
        Effect effect;
        effect.effectType = Effect::Type::Vibrato;
        config.effectVibrato = effect;
        config.effectVibrato->sampleRate = SAMPLERATE;
      }
      config.effectVibrato->vibratoConfig.depth = std::stof(argv[i + 1]);
    } else if (arg == "--vibrato_frequency" && i + 1 < argc) {
      if (!config.effectVibrato) {
        Effect effect;
        effect.effectType = Effect::Type::Vibrato;
        config.effectVibrato = effect;
        config.effectVibrato->sampleRate = SAMPLERATE;
      }
      config.effectVibrato->vibratoConfig.frequency = std::stof(argv[i + 1]);
    } else if (arg == "--tremolo") {
      Effect effect;
      effect.effectType = Effect::Type::Tremolo;
      config.effectTremolo = effect;
      config.effectTremolo->sampleRate = SAMPLERATE;
    } else if (arg == "--tremolo_depth" && i + 1 < argc) {
      if (!config.effectTremolo) {
        Effect effect;
        effect.effectType = Effect::Type::Tremolo;
        config.effectTremolo = effect;
        config.effectTremolo->sampleRate = SAMPLERATE;
      }
      config.effectTremolo->tremoloConfig.depth = std::stof(argv[i + 1]);
    } else if (arg == "--tremolo_frequency" && i + 1 < argc) {
      if (!config.effectTremolo) {
        Effect effect;
        effect.effectType = Effect::Type::Tremolo;
        config.effectTremolo = effect;
        config.effectTremolo->sampleRate = SAMPLERATE;
      }
      config.effectTremolo->tremoloConfig.frequency = std::stof(argv[i + 1]);
    } else if (arg == "--lowpass" && i + 1 < argc) {
      Effect effect;
      effect.effectType = Effect::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = SAMPLERATE;
      IIR lowPass = IIRFilters::lowPass(config.effectIIR->sampleRate,
                                        std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--highpass" && i + 1 < argc) {
      Effect effect;
      effect.effectType = Effect::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = SAMPLERATE;
      IIR lowPass = IIRFilters::highPass(config.effectIIR->sampleRate,
                                         std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--chorus_delay" && i + 1 < argc) {
      if (!config.effectChorus) {
        Effect effect;
        effect.effectType = Effect::Type::Chorus;
        config.effectChorus = effect;
        config.effectChorus->sampleRate = SAMPLERATE;
      }
      config.effectChorus->chorusConfig.delay = std::stof(argv[i + 1]);
    } else if (arg == "--chorus_depth" && i + 1 < argc) {
      if (!config.effectChorus) {
        Effect effect;
        effect.effectType = Effect::Type::Chorus;
        config.effectChorus = effect;
        config.effectChorus->sampleRate = SAMPLERATE;
      }
      config.effectChorus->chorusConfig.depth = std::stof(argv[i + 1]);
    } else if (arg == "--chorus_voices" && i + 1 < argc) {
      if (!config.effectChorus) {
        Effect effect;
        effect.effectType = Effect::Type::Chorus;
        config.effectChorus = effect;
        config.effectChorus->sampleRate = SAMPLERATE;
      }
      config.effectChorus->chorusConfig.numVoices = std::atoi(argv[i + 1]);
    } else if (arg == "--parallelization" && i + 1 < argc) {
      config.parallelization = std::atoi(argv[i + 1]);
    } else if (arg == "--midi" && i + 1 < argc) {
      config.midiFile = argv[i + 1];
    } else if (arg == "-r" || arg == "--reverb" && i + 1 < argc) {
      FIR fir(SAMPLERATE);
      fir.loadFromFile(argv[i + 1]);
      fir.setNormalization(true);
      Effect effect;
      effect.sampleRate = SAMPLERATE;
      effect.effectType = Effect::Type::Fir;
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
  KeyboardStream stream(BUFFER_SIZE, SAMPLERATE);
  bool running = true;

  stream.startKeypressWatchdog();

  SDL_AudioSpec desired, obtained;
  SDL_zero(desired);
  desired.freq = SAMPLERATE;
  desired.format = AUDIO_S16SYS;
  desired.channels = 1;
  desired.samples = BUFFER_SIZE;
  desired.callback = audioCallback;
  desired.userdata = &stream;

  if (SDL_OpenAudio(&desired, &obtained) < 0) {
    std::cerr << "SDL_OpenAudio failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_PauseAudio(0); // Start audio

  int rankIndex = 0;
  std::vector<Sound::Rank::Preset> presets = {
      Sound::Rank::Preset::Sine,
      Sound::Rank::Preset::Saw,
      Sound::Rank::Preset::Square,
      Sound::Rank::Preset::Triangular,
      Sound::Rank::Preset::SuperSaw,
      Sound::Rank::Preset::FatTriangle,
      Sound::Rank::Preset::PulseSquare,
      Sound::Rank::Preset::SineSawDrone,
      Sound::Rank::Preset::SuperSawWithSub,
      Sound::Rank::Preset::GlitchMix,
      Sound::Rank::Preset::OrganTone,
      Sound::Rank::Preset::LushPad,
      Sound::Rank::Preset::RetroLead,
      Sound::Rank::Preset::BassGrowl,
      Sound::Rank::Preset::AmbientDrone,
      Sound::Rank::Preset::SynthStab,
      Sound::Rank::Preset::GlassBells};

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
    stream.loadSoundMap(config.waveFile);
    config.waveForm = Sound::WaveForm::WaveFile;
  }
  stream.setLoaderFunc(loaderFunc);
  stream.setVolume(config.volume);
  std::vector<Effect> effects;
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
  stream.prepareSound(SAMPLERATE, config.adsr, config.rankPreset, effects,
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
    int totalSamples = SAMPLERATE * totalDuration;
    std::map<int, std::vector<std::string>> notesMap;

    for (int track = 0; track < midiFile.getTrackCount(); track++) {
      for (int event = 0; event < midiFile[track].size(); event++) {
        if (midiFile[track][event].isNoteOn()) {
          int note = midiFile[track][event][1];
          float frequency = noteToFreq(note);

          std::string noteKey = notes::getClosestNote(frequency);

          float duration = midiFile[track][event].getDurationInSeconds();
          float startTime = midiFile[track][event].seconds;

          int startSample = SAMPLERATE * startTime;

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
        stream.registerNote(note);
      }
    }

  } else {

    config.printConfig();
    stream.printInstructions();

    while (true) {
      int ch = getch();
      if (ch == 4) {
        printw("CTRL-D - shutting down\n");
        break;
      }
      stream.registerButtonPress(ch);
      if (ch == 'o' || ch == 'p') {
        clear();
        config.printConfig();
        stream.printInstructions();
      } else if (ch == 'O' || ch == 'P') {
        stream.teardown();
        printw("Updating the keyboard..\n");
        // stream.setup(maxPolyphony);
        if (ch == 'P') {
          rankIndex = (rankIndex + 1) % presets.size();
        } else {
          rankIndex = (rankIndex + presets.size() - 1) % presets.size();
        }
        stream.prepareSound(SAMPLERATE, config.adsr, presets[rankIndex],
                            effects, config.parallelization);
        clear();
        config.rankPreset = presets[rankIndex];
        config.printConfig();
        stream.printInstructions();
        printw("Updated to new preset %s\n",
               Sound::Rank::presetStr(presets[rankIndex]).c_str());
      }
    }
  }

  endwin(); // End curses mode
  SDL_CloseAudio();
  SDL_Quit();

  return 0;
}
