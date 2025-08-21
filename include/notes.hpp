#ifndef KEYBOARD_NOTES_HPP
#define KEYBOARD_NOTES_HPP

#include <json.hpp>
#include <map>
#include <string>
#include <vector>

#pragma once
namespace notes {

enum class TuningSystem { EqualTemperament, WerckmeisterIII };

// --- JSON serialization for TuningSystem ---
inline void to_json(nlohmann::json &j, const TuningSystem &t) {
  switch (t) {
  case TuningSystem::EqualTemperament:
    j = "EqualTemperament";
    break;
  case TuningSystem::WerckmeisterIII:
    j = "WerckmeisterIII";
    break;
  }
}

inline void from_json(const nlohmann::json &j, TuningSystem &t) {
  const std::string s = j.get<std::string>();
  if (s == "EqualTemperament") {
    t = TuningSystem::EqualTemperament;
  } else if (s == "WerckmeisterIII") {
    t = TuningSystem::WerckmeisterIII;
  } else {
    throw std::invalid_argument("Invalid TuningSystem: " + s);
  }
}

// Convert enum → string
inline std::string tuning_to_string(TuningSystem ts) {
  switch (ts) {
  case TuningSystem::EqualTemperament:
    return "EqualTemperament";
  case TuningSystem::WerckmeisterIII:
    return "WerckmeisterIII";
  }
  throw std::invalid_argument("Invalid TuningSystem enum");
}

double getFrequency(const std::string &note, TuningSystem ts);
std::vector<std::string> getNotes(TuningSystem ts);
int getNumberOfNotes(TuningSystem ts);
std::string getClosestNote(float frequency, TuningSystem ts);
std::map<std::string, double> &lookupFrequencies(notes::TuningSystem ts);
} // namespace notes

#endif
