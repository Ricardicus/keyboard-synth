#include <vector>
#include <iostream>
#include <fstream>
#include "waveread.hpp"

std::vector<char> readWavData(const std::string& filename, int &sampleRate) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open the file!" << std::endl;
        return {};
    }

    WavHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    sampleRate = header.sampleRate;

    // Additional verification can be done here, like checking if the header.riff contains "RIFF", etc.

    std::vector<char> data(header.dataSize);
    file.read(data.data(), header.dataSize);

    return data;
}
