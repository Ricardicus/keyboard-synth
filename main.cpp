#include <ncurses.h>
#include <stdio.h>

#include "adsr.hpp"
#include "keyboard.hpp"

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf(
      "   --form: form of sound [sine (default), triangular, saw, square]\n");
  printf("\n");
  printf("%s compiled %s %s\n", argv0, __DATE__, __TIME__);
}

int parseArguments(int argc, char *argv[], ADSR &adsr,
                   Sound::WaveForm &waveForm) {
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
    } else if (arg == "-h" || arg == "--help") {
      printHelp(argv[0]);
      return -1;
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int sampleRate = 44100;
  float duration = 0.8f;
  short amplitude = 32767;
  int maxPolyphony = 10;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(sampleRate * duration));
  Keyboard keyboard(maxPolyphony);

  Sound::WaveForm waveForm = Sound::WaveForm::Sine; // default waveform
  int c = parseArguments(argc, argv, adsr, waveForm);
  if (c < 0) {
    return 0;
  } else if (c > 0) {
    return 1;
  }

  keyboard.prepareSound(sampleRate, adsr, waveForm);

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
