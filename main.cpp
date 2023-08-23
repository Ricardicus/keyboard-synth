#include <ncurses.h>
#include <stdio.h>

#include "adsr.hpp"
#include "effects.hpp"
#include "keyboard.hpp"

constexpr int sampleRate = 44100;

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf(
      "   --form: form of sound [sine (default), triangular, saw, square]\n");
  printf("       -e: Add an echo effect\n");
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
int parseArguments(int argc, char *argv[], ADSR &adsr,
                   Sound::WaveForm &waveForm, Effects &effects) {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--form") {
      if (i + 1 < argc) {
        std::string form = argv[i + 1];
        if (form == "triangular") {
          waveForm = Sound::WaveForm::Triangular;
        } else if (form == "saw") {
          waveForm = Sound::WaveForm::Saw;
        } else if (form == "square") {
          waveForm = Sound::WaveForm::Square;
        }
      }
    } else if (arg == "-e" || arg == "--echo") {
      FIR fir(sampleRate);
      fir.setResonance({1.0, 0.5, 0.25, 0.125, 0.0515, 0.02575}, 1.0);
      effects.addFIR(fir);
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
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(sampleRate * duration));
  Keyboard keyboard(maxPolyphony);

  Sound::WaveForm waveForm = Sound::WaveForm::Sine; // default waveform
  Effects effects;
  int c = parseArguments(argc, argv, adsr, waveForm, effects);
  if (c < 0) {
    return 0;
  } else if (c > 0) {
    return 1;
  }

  printf("Processing buffers... preparing sound..\n");
  keyboard.setLoaderFunc(loaderFunc);
  keyboard.prepareSound(sampleRate, adsr, waveForm, effects);
  printf("Sound OK!\n");

  initscr();            // Initialize the library
  cbreak();             // Line buffering disabled
  keypad(stdscr, TRUE); // Enable special keys
  noecho();             // Don't show the key being pressed
  scrollok(stdscr, TRUE);

  printw("Keyboard sound: %s\n\nUse the keyboard buttons to play.\n\n",
         Sound::typeOfWave(waveForm).c_str());
  keyboard.printInstructions();
  while (true) {
    int ch = getch();
    keyboard.registerButtonPress(ch);
  }

  endwin(); // End curses mode
  return 0;
}
