#include "waveread.hpp"
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

bool isBigEndian() {
  int a = 1;
  return !((char *)&a)[0];
}

int convertToInt(char *buffer, int len) {
  int a = 0;
  if (!isBigEndian())
    for (int i = 0; i < len; i++)
      ((char *)&a)[i] = buffer[i];
  else
    for (int i = 0; i < len; i++)
      ((char *)&a)[3 - i] = buffer[i];
  return a;
}

char *loadWAV(std::string filename, int &chan, int &samplerate, int &bps,
              int &size) {
  printf("filename: %s\n", filename.c_str());
  const char *fn = filename.c_str();
  char buffer[4];
  std::ifstream in(fn, std::ios::binary);
  in.read(buffer, 4);
  if (strncmp(buffer, "RIFF", 4) != 0) {
    std::cout << "this is not a valid WAVE file" << std::endl;
    printf("Buffer: '%c' '%c' '%c' '%c'\n", buffer[0], buffer[1], buffer[2],
           buffer[3]);
    return NULL;
  }
  in.read(buffer, 4);
  in.read(buffer, 4); // WAVE
  in.read(buffer, 4); // fmt
  in.read(buffer, 4); // 16
  in.read(buffer, 2); // 1
  in.read(buffer, 2);
  chan = convertToInt(buffer, 2);
  in.read(buffer, 4);
  samplerate = convertToInt(buffer, 4);
  in.read(buffer, 4);
  in.read(buffer, 2);
  in.read(buffer, 2);
  bps = convertToInt(buffer, 2);

  char *buff = NULL;
  bool dataRead = false;
  do {
    if (buff != NULL)
      delete buff;
    in.read(buffer, 4); // data
    if (buffer[0] == 'd' && buffer[1] == 'a' && buffer[2] == 't' &&
        buffer[3] == 'a') {
      dataRead = true;
    }
    in.read(buffer, 4);
    int size_data = convertToInt(buffer, 4);
    size = size_data;
    buff = new char[size];
    in.read(buff, size);
  } while (!dataRead);
  return buff;
}
