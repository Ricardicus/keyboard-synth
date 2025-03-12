#ifndef KEYBOARD_NOTES_HPP
#define KEYBOARD_NOTES_HPP

#include <map>
#include <string>
#include <vector>

#pragma once
namespace notes {
double getFrequency(std::string &note);
std::vector<std::string> getNotes();
int getNumberOfNotes();
std::string getClosestNote(float frequency);
} // namespace notes

#endif
