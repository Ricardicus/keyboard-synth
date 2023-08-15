#include <ncurses.h>
#include <stdio.h>

#include "adsr.hpp"
#include "keyboard.hpp"

int main() {
  initscr();            // Initialize the library
  cbreak();             // Line buffering disabled
  keypad(stdscr, TRUE); // Enable special keys
  noecho();             // Don't show the key being pressed
  scrollok(stdscr, TRUE);
  int sampleRate = 44100;
  float duration = 0.8f;
  short amplitude = 32767;
  int maxPolyphony = 10;
  printw("Constructor adsr\n");
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(sampleRate * duration));
  Keyboard keyboard(maxPolyphony);

  printw("Preparing sound\n");
  keyboard.prepareSound(sampleRate, adsr, Sound::WaveForm::Sine);
  printw("Sound prepared!\n");
  printw("Keyboard started!\nUse the keyboard to play.\n\n");
  keyboard.printInstructions();
  while (true) {
    int ch = getch();
    keyboard.registerButtonPress(ch);
  }

  endwin(); // End curses mode
  return 0;
}
