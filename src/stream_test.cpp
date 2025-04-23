#include <SDL2/SDL.h>
#include <cmath>
#include <iostream>
#include <mutex>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>

#include "keyboardstream.hpp"

const int BUFFER_SIZE = 512;
const int SAMPLE_RATE = 44100;

void audioCallback(void *userdata, Uint8 *stream, int len) {
  auto *ks = static_cast<KeyboardStream *>(userdata);
  short *streamBuf = reinterpret_cast<short *>(stream);
  int samples = len / sizeof(short);
  ks->fillBuffer(streamBuf, samples);
}

// Enable raw mode on stdin (non-blocking, no echo)
void setTerminalRaw(bool enable) {
  static struct termios oldt, newt;
  if (enable) {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // disable buffering and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  } else {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restore
  }
}

int main(int argc, char *argv[]) {
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL Init Error: " << SDL_GetError() << std::endl;
    return 1;
  }

  KeyboardStream stream(BUFFER_SIZE, SAMPLE_RATE);
  stream.startKeypressWatchdog();

  SDL_AudioSpec desired, obtained;
  SDL_zero(desired);
  desired.freq = SAMPLE_RATE;
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

  std::cout
      << "Press a/s/d/f to play notes. Shift key to stop. Press q to quit.\n";

  setTerminalRaw(true); // enable immediate input

  bool running = true;
  while (running) {
    int c = getc(stdin);
    if (c == EOF)
      continue;
    printf("got %c\n", c);
    if (c == 4) break;
    stream.registerButtonPress(c);
  }

  setTerminalRaw(false); // restore terminal mode
  SDL_CloseAudio();
  SDL_Quit();
  return 0;
}
