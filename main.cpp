#include "MidiFile.h"

#include <chrono>
#include <cmath>
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

constexpr int SAMPLERATE = 44100;

typedef struct PlayConfig {
  ADSR *adsr;
  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Effects effects;
  std::string waveFile;
  std::string midiFile;
  float volume = 1.0;
} PlayConfig;

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf("   --form: form of sound [sine (default), triangular, saw, "
         "square]\n");
  printf("   -e|--echo: Add an echo effect\n");
  printf("   -r|--reverb [file]: Add a reverb effect based on IR response in "
         "this wav file\n");
  printf("   --file [file]: Use .wav files for notes with this mapping as "
         "provided in this file\n");
  printf("   --midi [file]: Play this MIDI (.mid) file");
  printf("   --volume [float]: Set the volume knob (default 1.0)\n");
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
        }
      }
    } else if (arg == "--file" && i + 1 < argc) {
      config.waveFile = argv[i + 1];
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
    } else if (arg == "--volume" && i + 1 < argc) {
      config.volume = std::stof(argv[i + 1]);
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
  config.adsr = &adsr;
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
  keyboard.prepareSound(SAMPLERATE, *config.adsr, config.waveForm,
                        config.effects);
  printf("Sound OK!\n");

  printf("Keyboard sound: %s\n\nUse the keyboard buttons to play.\n\n",
         Sound::typeOfWave(config.waveForm).c_str());

  if (config.midiFile.size() > 0) {
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

    printf("Playing the midi file %s\n", config.midiFile.c_str());

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
    initscr();            // Initialize the library
    cbreak();             // Line buffering disabled
    keypad(stdscr, TRUE); // Enable special keys
    noecho();             // Don't show the key being pressed
    scrollok(stdscr, TRUE);
    keyboard.printInstructions();

    while (true) {
      int ch = getch();
      keyboard.registerButtonPress(ch);
    }

    endwin(); // End curses mode
  }

  return 0;
}
