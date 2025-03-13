#include "MidiFile.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <ncurses.h>
#include <thread>
#include <vector>

#include <stdio.h>

#include "adsr.hpp"
#include "effects.hpp"
#include "keyboard.hpp"

#define SAMPLE_RATE 44100

// MIDI note number to frequency
float noteToFreq(int note) { return 440.0f * pow(2, (note - 69) / 12.0f); }

bool fileExists(const std::string &file) {
  return std::filesystem::exists(file);
}

constexpr int SAMPLERATE = 44100;

class PlayConfig {
public:
  ADSR adsr;
  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Sound::Rank::Preset rankPreset = Sound::Rank::Preset::None;
  Effects effects;
  std::string waveFile;
  std::string midiFile;
  float volume = 1.0;
  float duration = 0.8f;
  std::string getPrintableConfig() {
    std::string result;
    result += "Keyboard sound configuration:\n";
    result += "  volume: " + std::to_string(volume) + "\n";
    result +=
        "  notes-wave-map: " + (waveFile.size() > 0 ? waveFile : "none") + "\n";
    result += "  waveform: " + Sound::typeOfWave(waveForm) + "\n";
    result += "  ADSR:\n";
    result += "    amplitude: " + std::to_string(adsr.amplitude) + "\n";
    result += "    quantas: " + std::to_string(adsr.quantas) + "\n";
    result += "    qadsr: " + std::to_string(adsr.qadsr[0]) + " " +
              std::to_string(adsr.qadsr[1]) + " " +
              std::to_string(adsr.qadsr[2]) + " " +
              std::to_string(adsr.qadsr[3]) + "\n";
    result += "    length: " + std::to_string(adsr.length) + "\n";
    result +=
        "    quantas_length: " + std::to_string(adsr.quantas_length) + "\n";
    result += "    sustain_level: " + std::to_string(adsr.sustain_level) + "\n";
    result += "    visualization: [see below]\n";
    result += adsr.getCoolASCIVisualization("    ");
    result += "  FIRs: " + std::to_string(effects.firs.size()) + "\n";
    for (int i = 0; i < effects.firs.size(); i++) {
      result += "    [" + std::to_string(i + 1) +
                "] IR length: " + std::to_string(effects.firs[i].getIRLen()) +
                ", normalized: " +
                (effects.firs[i].getNormalization() ? "true" : "false") + "\n";
    }
    return result;
  }

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
    printw("%f\n", volume);
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
    printw("%s\n", Sound::typeOfWave(waveForm).c_str());
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
    printw("%s\n", adsr.getCoolASCIVisualization("    ").c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  FIRs: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%lu\n", effects.firs.size());
    attroff(COLOR_PAIR(5));

    for (size_t i = 0; i < effects.firs.size(); i++) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("    [%lu] IR length: ", i + 1);
      attroff(A_BOLD | COLOR_PAIR(4));

      attron(COLOR_PAIR(5));
      printw("%zu, Normalized: %s\n", effects.firs[i].getIRLen(),
             effects.firs[i].getNormalization() ? "true" : "false");
      attroff(COLOR_PAIR(5));
    }

    refresh(); // Refresh the screen to apply changes
  }
};

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf("   --form: form of sound [sine (default), triangular, saw, supersaw,"
         "square]\n");
  printf("   -e|--echo: Add an echo effect\n");
  printf("   -r|--reverb [file]: Add a reverb effect based on IR response in "
         "this wav file\n");
  printf("   --notes [file]: Map notes to .wav files as mapped in this .json "
         "file\n");
  printf("   --midi [file]: Play this MIDI (.mid) file\n");
  printf("   --volume [float]: Set the volume knob (default 1.0)\n");
  printf("   --duration [float]: Note duration in seconds (default 0.8)\n");
  printf("   --adsr [int,int,int,int]: Set the ADSR quant intervals "
         "comma-separated (default 1,1,3,3)\n");
  printf("   --sustain [float]: Set the sustain level [0,1] (default 0.8)\n");
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
          config.waveForm = Sound::WaveForm::Triangular;
        } else if (form == "saw") {
          config.waveForm = Sound::WaveForm::Saw;
        } else if (form == "square") {
          config.waveForm = Sound::WaveForm::Square;
        } else if (form == "supersaw") {
          config.rankPreset = Sound::Rank::Preset::SuperSaw;
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
      config.effects.addFIR(fir);
    } else if (arg == "--midi" && i + 1 < argc) {
      config.midiFile = argv[i + 1];
    } else if (arg == "-r" || arg == "--reverb" && i + 1 < argc) {
      FIR fir(SAMPLERATE);
      fir.loadFromFile(argv[i + 1]);
      fir.setNormalization(true);
      config.effects.addFIR(fir);
    } else if (arg == "--sustain" && i + 1 < argc) {
      config.adsr.sustain_level =
          std::stof(argv[i + 1]) * config.adsr.amplitude;
    } else if (arg == "--volume" && i + 1 < argc) {
      config.volume = std::stof(argv[i + 1]);
    } else if (arg == "--duration" && i + 1 < argc) {
      config.duration = std::stof(argv[i + 1]);
      config.adsr.setLength(static_cast<int>(SAMPLERATE * config.duration));
    } else if (arg == "-h" || arg == "--help") {
      printHelp(argv[0]);
      return -1;
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  float duration = 0.8f;
  short amplitude = 32767;
  int maxPolyphony = 50;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(SAMPLERATE * duration));
  Keyboard keyboard(maxPolyphony);

  PlayConfig config;
  config.adsr = adsr;
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
  if (config.rankPreset != Sound::Rank::Preset::None) {
    keyboard.prepareSound(SAMPLERATE, config.adsr, config.rankPreset,
                          config.effects);
  } else {
    keyboard.prepareSound(SAMPLERATE, config.adsr, config.waveForm,
                          config.effects);
  }
  printf("\nSound OK!\n");
  initscr();            // Initialize the library
  cbreak();             // Line buffering disabled
  keypad(stdscr, TRUE); // Enable special keys
  noecho();             // Don't show the key being pressed
  scrollok(stdscr, TRUE);

  std::string printableConfig = config.getPrintableConfig();

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
      }
    }
  }

  endwin(); // End curses mode

  return 0;
}
