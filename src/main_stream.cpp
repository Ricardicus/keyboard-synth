#include "MidiFile.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <ncurses.h>
#include <optional>
#include <stdio.h>
#include <thread>
#include <vector>

#include <SDL2/SDL.h>
#include <civetweb.h>
#include <json.hpp>

using json = nlohmann::json;

#include "adsr.hpp"
#include "effect.hpp"
#include "keyboardstream.hpp"

constexpr int BUFFER_SIZE = 512;

static std::string utc_iso8601() {
  using namespace std::chrono;
  auto now = system_clock::now();
  std::time_t tt = system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32 // MSVC secure variants
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

int presets_api_handler(struct mg_connection *conn, void *cbdata) {
  const struct mg_request_info *req = mg_get_request_info(conn);
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);
  const std::filesystem::path presetPath = "synths/keyboard_presets.json";
  kbs->lock();

  /* Only POST is allowed ---------------------------------------------------*/
  if (std::string{req->request_method} != "POST") {
    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
    kbs->unlock();
    return 405;
  }

  /* Read request body ------------------------------------------------------*/
  char buf[2048];
  int n = mg_read(conn, buf, sizeof(buf) - 1);
  buf[n] = '\0';
  bool updated = false;

  json body;
  try {
    body = json::parse(buf);
  } catch (...) {
    mg_printf(conn,
              "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\":\"Invalid JSON\"}");
    kbs->unlock();
    return 400;
  }
  if (!body.contains("method") || !body["method"].is_string()) {
    mg_printf(conn,
              "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\":\"'method' field "
              "required\"}");
    kbs->unlock();
    return 400;
  }
  const std::string method = body["method"];

  if (method == "save") {
    /* Validate
     * ---------------------------------------------------------------*/
    if (!body.contains("name") || !body["name"].is_string()) {
      mg_printf(conn,
                "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\":\"'name' field "
                "required\"}");
      kbs->unlock();
      return 400;
    }
    const std::string presetName = body["name"];
    /* Compose the preset record ---------------------------------------------*/
    json preset = {{"name", presetName},
                   {"datetime", utc_iso8601()},
                   {"configuration", kbs->toJson()}};
    /* File paths
     * -------------------------------------------------------------*/
    std::filesystem::create_directories(
        presetPath.parent_path()); // make sure ./synths exists

    /* Load / create wrapper object { "presets": [] }
     * -------------------------*/
    json fileObj;
    if (std::ifstream in{presetPath}; in.good()) {
      try {
        in >> fileObj;
      } catch (...) { /* corrupted file – start fresh */
      }
    }
    if (!fileObj.contains("presets") || !fileObj["presets"].is_array())
      fileObj["presets"] = json::array();

    /* Update or insert
     * -------------------------------------------------------*/
    for (auto &p : fileObj["presets"]) {
      if (p.contains("name") && p["name"] == presetName) {
        p = preset; // overwrite whole record
        updated = true;
        break;
      }
    }
    if (!updated)
      fileObj["presets"].push_back(preset);

    /* Write back atomically
     * --------------------------------------------------*/
    {
      const auto tmp = presetPath.string() + ".tmp";
      std::ofstream out{tmp, std::ios::trunc};
      out << fileObj.dump(2);
      out.close();
      std::filesystem::rename(tmp, presetPath); // replace original
    }
  } else if (method == "load") {
    /* Validate
     * ---------------------------------------------------------------*/
    if (!body.contains("preset") || !body["preset"].is_string()) {
      mg_printf(conn,
                "HTTP/1.1 400 Bad Request\r\n\r\n{\"error\":\"'preset' field "
                "required in request body\"}");
      kbs->unlock();
      return 400;
    }
    const std::string presetName = body["preset"];
    /* Compose the preset record ---------------------------------------------*/
    json preset = {{"name", presetName},
                   {"datetime", utc_iso8601()},
                   {"configuration", kbs->toJson()}};
    /* Load / create wrapper object { "presets": [] }
     * -------------------------*/
    json fileObj;
    if (std::ifstream in{presetPath}; in.good()) {
      try {
        in >> fileObj;
      } catch (...) {
        mg_printf(
            conn,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
            "{\"status\":\"failed\",\"message\":\"invalid presets file\"}");
        kbs->unlock();
        return 200;
      }
    }
    if (!fileObj.contains("presets") || !fileObj["presets"].is_array())
      fileObj["presets"] = json::array();

    /* Update or insert
     * -------------------------------------------------------*/
    for (auto &p : fileObj["presets"]) {
      if (p.contains("name") && p["name"] == presetName &&
          p.contains("configuration")) {
        if (!kbs->loadJson(p["configuration"])) {
          mg_printf(conn,
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                    "{\"status\":\"ok\",\"message\":\"Preset loaded\"}");
          kbs->unlock();
          return 200;
        } else {
          mg_printf(conn,
                    "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                    "{\"status\":\"failed\",\"message\":\"Invalid preset\"}");
          kbs->unlock();
          return 200;
        }
      }
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
                    "{\"status\":\"failed\",\"message\":\"Preset not found\"}");
    kbs->unlock();
    return 200;
  } else if (method == "list") {

    /* Load / create wrapper object { "presets": [] }
     * -------------------------*/
    json fileObj;
    if (std::ifstream in{presetPath}; in.good()) {
      try {
        in >> fileObj;
      } catch (...) {
        mg_printf(
            conn,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
            "{\"status\":\"failed\",\"message\":\"invalid presets file\"}");
        kbs->unlock();
        return 200;
      }
    }
    if (!fileObj.contains("presets") || !fileObj["presets"].is_array())
      fileObj["presets"] = json::array();

    /* Update or insert
     * -------------------------------------------------------*/
    json response;
    std::vector<json> names;
    for (auto &p : fileObj["presets"]) {
      if (p.contains("name")) {
        json entry;
        entry["name"] = p["name"];
        names.push_back(entry);
      }
    }
    response["status"] = "ok";
    response["presets"] = names;
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
              "%s",
              response.dump().c_str());
    kbs->unlock();
    return 200;
  }
  /* Done -------------------------------------------------------------------*/
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
            "{\"status\":\"ok\",\"updated\":%s}",
            updated ? "true" : "false");
  kbs->unlock();
  return 200;
}

int input_push_handler(struct mg_connection *conn, void *cbdata) {
  refresh();
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);

  char buffer[128];
  int len = mg_read(conn, buffer, sizeof(buffer) - 1);
  buffer[len] = '\0';

  try {
    json body = json::parse(buffer);
    if (!body.contains("key") || !body["key"].is_string()) {
      mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nMissing 'key'");
      return 400;
    }

    std::string note = body["key"];
    kbs->registerNote(note);

    mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
    return 200;

  } catch (...) {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON");
    return 400;
  }
}

int input_release_handler(struct mg_connection *conn, void *cbdata) {
  refresh();
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);

  char buffer[128];
  int len = mg_read(conn, buffer, sizeof(buffer) - 1);
  buffer[len] = '\0';

  try {
    json body = json::parse(buffer);
    if (!body.contains("key") || !body["key"].is_string()) {
      mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nMissing 'key'");
      return 400;
    }

    std::string note = body["key"];
    kbs->registerNoteRelease(note);

    mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
    return 200;

  } catch (...) {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON");
    return 400;
  }
}

int config_api_handler(struct mg_connection *conn, void *cbdata) {
  const struct mg_request_info *req_info = mg_get_request_info(conn);
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);
  kbs->lock();

  if (std::string(req_info->request_method) == "GET") {
    // Send current config
    std::optional<std::reference_wrapper<EchoEffect<float>>> echoConf =
        std::nullopt;
    std::optional<std::reference_wrapper<Effect<float>::VibratoConfig>>
        vibConf = std::nullopt;
    std::optional<std::reference_wrapper<Effect<float>::TremoloConfig>>
        tremConf = std::nullopt;
    for (int e = 0; e < kbs->effects.size(); e++) {
      if (auto echo = std::get_if<EchoEffect<float>>(&kbs->effects[e].config)) {
        echoConf = std::ref(*echo);
      }
      if (auto echo = std::get_if<Effect<float>::VibratoConfig>(
              &kbs->effects[e].config)) {
        vibConf = std::ref(*echo);
      }
      if (auto echo = std::get_if<Effect<float>::TremoloConfig>(
              &kbs->effects[e].config)) {
        tremConf = std::ref(*echo);
      }
    }
    json response;
    if (echoConf && vibConf && tremConf) {
      response = {{"gain", kbs->gain},
                  {"adsr",
                   {{"attack", kbs->adsr.qadsr[0]},
                    {"decay", kbs->adsr.qadsr[1]},
                    {"sustain", kbs->adsr.qadsr[2]},
                    {"release", kbs->adsr.qadsr[3]}}},
                  {"echo",
                   {{"rate", echoConf->get().getRate()},
                    {"feedback", echoConf->get().getFeedback()},
                    {"mix", echoConf->get().getMix()},
                    {"sampleRate", echoConf->get().getSampleRate()}}},
                  {"tremolo",
                   {{"depth", tremConf->get().depth},
                    {"frequency", tremConf->get().frequency}}},
                  {"vibrato",
                   {{"depth", vibConf->get().depth},
                    {"frequency", vibConf->get().frequency}}},
                  {"highpass", kbs->effects[0].iirs[0].presentable},
                  {"lowpass", kbs->effects[0].iirs[1].presentable}};
    }

    std::string body = response.dump();
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
              "Content-Length: %zu\r\n\r\n%s",
              body.size(), body.c_str());
    kbs->unlock();
    return 200;

  } else if (std::string(req_info->request_method) == "POST") {
    char buffer[1024];
    int len = mg_read(conn, buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';

    try {
      json body = json::parse(buffer);

      if (body.contains("gain") && body["gain"].is_number()) {
        kbs->gain = body["gain"];
      }

      std::optional<std::reference_wrapper<EchoEffect<float>>> echoConf =
          std::nullopt;
      std::optional<std::reference_wrapper<Effect<float>::VibratoConfig>>
          vibConf = std::nullopt;
      std::optional<std::reference_wrapper<Effect<float>::TremoloConfig>>
          tremConf = std::nullopt;
      for (int e = 0; e < kbs->effects.size(); e++) {
        if (auto echo =
                std::get_if<EchoEffect<float>>(&kbs->effects[e].config)) {
          echoConf = std::ref(*echo);
        }
        if (auto echo = std::get_if<Effect<float>::VibratoConfig>(
                &kbs->effects[e].config)) {
          vibConf = std::ref(*echo);
        }
        if (auto echo = std::get_if<Effect<float>::TremoloConfig>(
                &kbs->effects[e].config)) {
          tremConf = std::ref(*echo);
        }
      }

      if (echoConf) {
        if (body.contains("echo") && body["echo"].is_object()) {
          json echo = body["echo"];
          if (echo.contains("rate"))
            echoConf->get().setRate(echo["rate"]);
          if (echo.contains("feedback"))
            echoConf->get().setFeedback(echo["feedback"]);
          if (echo.contains("mix"))
            echoConf->get().setMix(echo["mix"]);
          if (echo.contains("sampleRate"))
            echoConf->get().setSampleRate(echo["sampleRate"]);
        }
      }

      if (body.contains("adsr") && body["adsr"].is_object()) {
        json adsr = body["adsr"];
        if (adsr.contains("attack"))
          kbs->adsr.qadsr[0] = adsr["attack"];
        if (adsr.contains("decay"))
          kbs->adsr.qadsr[1] = adsr["decay"];
        if (adsr.contains("sustain"))
          kbs->adsr.qadsr[2] = adsr["sustain"];
        if (adsr.contains("release"))
          kbs->adsr.qadsr[3] = adsr["release"];
        kbs->adsr.update_len();
      }

      if (tremConf && body.contains("tremolo") && body["tremolo"].is_object()) {
        json tremolo = body["tremolo"];
        if (tremolo.contains("depth")) {
          tremConf->get().depth = tremolo["depth"];
        }
        if (tremolo.contains("frequency")) {
          tremConf->get().frequency = tremolo["frequency"];
        }
      }

      if (vibConf && body.contains("vibrato") && body["vibrato"].is_object()) {
        json vibrato = body["vibrato"];
        if (vibrato.contains("depth")) {
          vibConf->get().depth = vibrato["depth"];
        }
        if (vibrato.contains("frequency")) {
          vibConf->get().frequency = vibrato["frequency"];
        }
      }

      if (body.contains("highpass") && body["highpass"].is_number()) {
        float cutoff = body["highpass"];
        IIR<float> hp = IIRFilters::highPass<float>(SAMPLERATE, cutoff);
        kbs->effects[0].iirs[0] = hp;
      }

      if (body.contains("lowpass") && body["lowpass"].is_number()) {
        float cutoff = body["lowpass"];
        IIR<float> lp = IIRFilters::lowPass<float>(SAMPLERATE, cutoff);
        kbs->effects[0].iirs[1] = lp;
      }

      kbs->copyEffectsToSynths();

      mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
      kbs->unlock();
      return 200;

    } catch (...) {
      mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON");
      kbs->unlock();
      return 400;
    }
  }

  mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
  kbs->unlock();
  return 405;
}

// API handler for /api/oscillators
int oscillator_api_handler(struct mg_connection *conn, void *cbdata) {
  const struct mg_request_info *req_info = mg_get_request_info(conn);
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);
  kbs->lock();

  if (std::string(req_info->request_method) == "GET") {
    json response = json::array();
    for (const auto &osc : kbs->synth) {
      response.push_back({{"volume", osc.volume},
                          {"octave", osc.octave},
                          {"detune", osc.detune},
                          {"sound", Sound::Rank<float>::presetStr(osc.sound)}});
    }

    std::string body = response.dump();
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
              "Content-Length: %zu\r\n\r\n%s",
              body.size(), body.c_str());
    kbs->unlock();
    return 200;

  } else if (std::string(req_info->request_method) == "POST") {
    char buffer[1024];
    int len = mg_read(conn, buffer, sizeof(buffer) - 1);
    buffer[len] = '\0';

    try {
      json body = json::parse(buffer);
      if (!body.contains("id") || !body["id"].is_number_integer()) {
        mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nMissing 'id'");

        kbs->unlock();
        return 400;
      }

      int id = body["id"];
      if (id < 0 || id >= (int)kbs->synth.size()) {
        mg_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\nInvalid ID");
        kbs->unlock();
        return 404;
      }

      if (body.contains("volume"))
        kbs->synth[id].volume = body["volume"];
      if (body.contains("sound")) {
        kbs->synth[id].sound = Sound::Rank<float>::fromString(body["sound"]);
        kbs->synth[id].initialize();
      }
      if (body.contains("octave"))
        kbs->synth[id].octave = body["octave"];
      if (body.contains("detune"))
        kbs->synth[id].detune = body["detune"];

      kbs->synth[id].updateFrequencies();

      mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
      kbs->unlock();
      return 200;

    } catch (...) {
      mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON");
      kbs->unlock();
      return 400;
    }
  }

  mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
  kbs->unlock();
  return 405;
}

// MIDI note number to frequency
float noteToFreq(int note) { return 440.0f * pow(2, (note - 69) / 12.0f); }

bool fileExists(const std::string &file) {
  return std::filesystem::exists(file);
}

void audioCallback(void *userdata, Uint8 *stream, int len) {
  auto *ks = static_cast<KeyboardStream *>(userdata);
  float *streamBuf = reinterpret_cast<float *>(stream);
  int samples = len / sizeof(float);
  ks->lock();
  ks->fillBuffer(streamBuf, samples);
  ks->unlock();
}

class PlayConfig {
public:
  ADSR adsr;
  Sound::WaveForm waveForm = Sound::WaveForm::Sine;
  Sound::Rank<float>::Preset rankPreset = Sound::Rank<float>::Preset::None;
  std::string waveFile;
  std::string midiFile;
  std::optional<Effect<float>> effectFIR = std::nullopt;
  std::optional<Effect<float>> effectChorus = std::nullopt;
  std::optional<Effect<float>> effectIIR = std::nullopt;
  std::optional<Effect<float>> effectVibrato = std::nullopt;
  std::optional<Effect<float>> effectTremolo = std::nullopt;
  EchoEffect<float> effectEcho{1.0, 0.3, 0.0, SAMPLERATE};
  float volume = 1.0;
  float duration = 0.1f;
  int parallelization = 8; // Number of threads to use in keyboard preparation

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
    printw("%.2f\n", volume);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  Sample rate: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%d\n", SAMPLERATE);
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
    printw("%s\n", rankPreset != Sound::Rank<float>::Preset::None
                       ? Sound::Rank<float>::presetStr(rankPreset).c_str()
                       : Sound::typeOfWave(waveForm).c_str());
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
    printw("%s", adsr.getCoolASCIVisualization("    ").c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);

    if (effectFIR) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  FIRs: ");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%lu\n", effectFIR->firs.size());
      attroff(COLOR_PAIR(5));
      for (size_t i = 0; i < effectFIR->firs.size(); i++) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] IR length: ", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        printw("%zu, Normalized: %s\n", effectFIR->firs[i].getIRLen(),
               effectFIR->firs[i].getNormalization() ? "true" : "false");
        attroff(COLOR_PAIR(5));
      }
    }

    if (effectIIR) {
      attron(A_BOLD | COLOR_PAIR(4));
      printw("  IIRs: ");
      attroff(A_BOLD | COLOR_PAIR(4));
      attron(COLOR_PAIR(5));
      printw("%lu\n", effectIIR->iirs.size());
      attroff(COLOR_PAIR(5));
      for (size_t i = 0; i < effectIIR->iirs.size(); i++) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] Memory: ", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        printw("%u\n", effectIIR->iirs[i].memory);
        attroff(COLOR_PAIR(5));
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] poles:", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        for (int a = 0; a < effectIIR->iirs[i].as.size(); a++) {
          printw(" %f", effectIIR->iirs[i].as[a]);
        }
        printw("\n");
        attroff(COLOR_PAIR(5));
        attron(A_BOLD | COLOR_PAIR(4));
        printw("    [%lu] zeroes:", i + 1);
        attroff(A_BOLD | COLOR_PAIR(4));

        attron(COLOR_PAIR(5));
        for (int b = 0; b < effectIIR->iirs[i].bs.size(); b++) {
          printw(" %f", effectIIR->iirs[i].bs[b]);
        }
        printw("\n");
        attroff(COLOR_PAIR(5));
      }
    }

    /* ------- Chorus
     * -----------------------------------------------------------*/
    if (effectChorus) {
      if (const auto *c =
              std::get_if<Effect<float>::ChorusConfig>(&effectChorus->config)) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("  Chorus: delay=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", c->delay);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("depth=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", c->depth);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("voices=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%d\n", c->numVoices);
        attroff(COLOR_PAIR(5));
      }
    }

    /* ------- Vibrato
     * ----------------------------------------------------------*/
    if (effectVibrato) {
      if (const auto *v = std::get_if<Effect<float>::VibratoConfig>(
              &effectVibrato->config)) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("  Vibrato: frequency=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", v->frequency);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("depth=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f\n", v->depth);
        attroff(COLOR_PAIR(5));
      }
    }

    /* ------- Tremolo
     * ----------------------------------------------------------*/
    if (effectTremolo) {
      if (const auto *t = std::get_if<Effect<float>::TremoloConfig>(
              &effectTremolo->config)) {
        attron(A_BOLD | COLOR_PAIR(4));
        printw("  Tremolo: frequency=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f ", t->frequency);
        attroff(COLOR_PAIR(5));

        attron(A_BOLD | COLOR_PAIR(4));
        printw("depth=");
        attroff(A_BOLD | COLOR_PAIR(4));
        attron(COLOR_PAIR(5));
        printw("%f\n", t->depth);
        attroff(COLOR_PAIR(5));
      }
    }

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  note length: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%.2f s\n", duration * adsr.quantas);
    attroff(COLOR_PAIR(5));

    attron(A_BOLD | COLOR_PAIR(4));
    printw("  A4 frequency: ");
    attroff(A_BOLD | COLOR_PAIR(4));
    attron(COLOR_PAIR(5));
    printw("%.2f Hz\n", notes::getFrequency("A4"));
    attroff(COLOR_PAIR(5));

    refresh(); // Refresh the screen to apply changes
  }
};

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf("   --form: form of sound [sine (default), triangular, saw, supersaw, "
         "\n");
  printf("           square, fattriangle, pulsesquare, sinesawdrone, "
         "supersawsub, \n");
  printf(
      "           glitchmix, lushpad, retroLead, bassgrowl, ambientdrone,\n");
  printf("           synthstab, glassbells, organtone]\n");
  printf("   -e|--echo: Add an echo effect\n");
  printf("   --chorus: Add a chorus effect with default settings\n");
  printf("   --chorus_delay [float]: Set the chorus delay factor, default: "
         "0.45\n");
  printf("   --chorus_depth [float]: Set the chorus depth factor, in pitch "
         "cents, default: "
         "3\n");
  printf("   --chorus_voices[int]: Set the chorus voices, default: 3\n");
  printf("   --vibrato: Add a vibrato effect with default settings\n");
  printf("   --vibrato_depth [float]: Set the vibrato depth factor, default: "
         "0.3\n");
  printf("   --vibrato_frequency [float]: Set the vibrato frequency, in Hertz "
         " default: 6\n");
  printf("   --tremolo: Add a tremolo effect with default settings\n");
  printf("   --tremolo_depth [float]: Set the tremolo depth factor [0-1], "
         "default: "
         "1.0\n");
  printf("   --tremolo_frequency [float]: Set the tremolo frequency, in Hertz "
         " default: 18\n");
  printf("   -r|--reverb [file]: Add a reverb effect based on IR response in "
         "this wav file\n");
  printf("   --notes [file]: Map notes to .wav files as mapped in this .json "
         "file\n");
  printf("   --midi [file]: Play this MIDI (.mid) file\n");
  printf("   --volume [float]: Set the volume knob (default 1.0)\n");
  printf("   --duration [float]: Note ADSR quanta duration in seconds (default "
         "0.1)\n");
  printf("   --adsr [int,int,int,int]: Set the ADSR quant intervals "
         "comma-separated (default 1,1,3,3)\n");
  printf("   --sustain [float]: Set the sustain level [0,1] (default 0.8)\n");
  printf(
      "   --lowpass [float]: Set the lowpass filter cut off frequency in Hz\n");
  printf("                   (default no low pass)\n");
  printf("   --highpass [float]: Set the highpass filter cut off frequency in "
         "Hz\n");
  printf("                (default no highpass)\n");
  printf("   --parallelization [int]: Number of threads used in keyboard "
         "preparation default: 8");
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
  auto ensureVibrato = [&](float defFreq = 6.0f, float defDepth = 0.3f) {
    if (!config.effectVibrato) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::Vibrato;
      e.config = Effect<float>::VibratoConfig{defFreq, defDepth};
      e.sampleRate = SAMPLERATE; // make sure SR is set once
      config.effectVibrato = e;
    }
  };
  auto ensureChorus = [&](float defDelay = 0.05f, float defDepth = 3.0f,
                          int defVoices = 3) {
    /* Create the effect only if it does not exist or is of another type */
    if (!config.effectChorus) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::Chorus;
      e.config = Effect<float>::ChorusConfig{defDelay, defDepth, defVoices};
      e.sampleRate = SAMPLERATE; // make sure SR is set once
      config.effectChorus = e;
    }
  };
  auto ensureTremolo = [&](float defFreq = 5.0f, float defDepth = 0.5f) {
    if (!config.effectTremolo) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::Tremolo;
      e.config = Effect<float>::TremoloConfig{defFreq, defDepth};
      e.sampleRate = SAMPLERATE; // set once
      config.effectTremolo = e;
    }
  };

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--form") {
      if (i + 1 < argc) {
        std::string form = argv[i + 1];
        if (form == "triangular") {
          config.rankPreset = Sound::Rank<float>::Preset::Triangular;
        } else if (form == "saw") {
          config.rankPreset = Sound::Rank<float>::Preset::Saw;
        } else if (form == "square") {
          config.rankPreset = Sound::Rank<float>::Preset::Square;
        } else if (form == "sine") {
          config.rankPreset = Sound::Rank<float>::Preset::Sine;
        } else if (form == "supersaw") {
          config.rankPreset = Sound::Rank<float>::Preset::SuperSaw;
        } else if (form == "fattriangle") {
          config.rankPreset = Sound::Rank<float>::Preset::FatTriangle;
        } else if (form == "pulsesquare") {
          config.rankPreset = Sound::Rank<float>::Preset::PulseSquare;
        } else if (form == "sinesawdrone") {
          config.rankPreset = Sound::Rank<float>::Preset::SineSawDrone;
        } else if (form == "supersawsub") {
          config.rankPreset = Sound::Rank<float>::Preset::SuperSawWithSub;
        } else if (form == "glitchmix") {
          config.rankPreset = Sound::Rank<float>::Preset::GlitchMix;
        } else if (form == "lushpad") {
          config.rankPreset = Sound::Rank<float>::Preset::LushPad;
        } else if (form == "retrolead") {
          config.rankPreset = Sound::Rank<float>::Preset::RetroLead;
        } else if (form == "bassgrowl") {
          config.rankPreset = Sound::Rank<float>::Preset::BassGrowl;
        } else if (form == "ambientdrone") {
          config.rankPreset = Sound::Rank<float>::Preset::AmbientDrone;
        } else if (form == "synthstab") {
          config.rankPreset = Sound::Rank<float>::Preset::SynthStab;
        } else if (form == "glassbells") {
          config.rankPreset = Sound::Rank<float>::Preset::GlassBells;
        } else if (form == "organtone") {
          config.rankPreset = Sound::Rank<float>::Preset::OrganTone;
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
      config.effectEcho.mix = 0.5f;
    } else if (arg == "--vibrato") { // enable with defaults
      ensureVibrato();               // only creates if missing

    } else if (arg == "--vibrato_depth" && i + 1 < argc) {
      ensureVibrato(); // keep existing freq
      auto &v =
          std::get<Effect<float>::VibratoConfig>(config.effectVibrato->config);
      v.depth = std::stof(argv[i + 1]); // change depth only

    } else if (arg == "--vibrato_frequency" && i + 1 < argc) {
      ensureVibrato(); // keep existing depth
      auto &v =
          std::get<Effect<float>::VibratoConfig>(config.effectVibrato->config);
      v.frequency = std::stof(argv[i + 1]); // change frequency only
    } else if (arg == "--tremolo") {        // enable with defaults
      ensureTremolo();
    } else if (arg == "--tremolo_depth" && i + 1 < argc) {
      ensureTremolo(); // keep existing freq
      auto &v =
          std::get<Effect<float>::TremoloConfig>(config.effectTremolo->config);
      v.depth = std::stof(argv[i + 1]);
    } else if (arg == "--tremolo_frequency" && i + 1 < argc) {
      ensureTremolo(); // keep existing depth
      auto &v =
          std::get<Effect<float>::TremoloConfig>(config.effectTremolo->config);
      v.frequency = std::stof(argv[i + 1]);
    } else if (arg == "--lowpass" && i + 1 < argc) {
      Effect<float> effect;
      effect.effectType = Effect<float>::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = SAMPLERATE;
      IIR<float> lowPass = IIRFilters::lowPass<float>(
          config.effectIIR->sampleRate, std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--highpass" && i + 1 < argc) {
      Effect<float> effect;
      effect.effectType = Effect<float>::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = SAMPLERATE;
      IIR<float> lowPass = IIRFilters::highPass<float>(
          config.effectIIR->sampleRate, std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--chorus") { // enable with defaults
      ensureChorus();
    } else if (arg == "--chorus_delay" && i + 1 < argc) {
      ensureChorus();
      auto &v =
          std::get<Effect<float>::ChorusConfig>(config.effectVibrato->config);
      v.delay = std::stof(argv[i + 1]);
    } else if (arg == "--chorus_depth" && i + 1 < argc) {
      ensureChorus();
      auto &v =
          std::get<Effect<float>::ChorusConfig>(config.effectVibrato->config);
      v.depth = std::stof(argv[i + 1]);
    } else if (arg == "--chorus_voices" && i + 1 < argc) {
      ensureChorus();
      auto &v =
          std::get<Effect<float>::ChorusConfig>(config.effectVibrato->config);
      v.numVoices = std::atoi(argv[i + 1]);
    } else if (arg == "--parallelization" && i + 1 < argc) {
      config.parallelization = std::atoi(argv[i + 1]);
    } else if (arg == "--midi" && i + 1 < argc) {
      config.midiFile = argv[i + 1];
    } else if (arg == "-r" || arg == "--reverb" && i + 1 < argc) {
      FIR fir(SAMPLERATE);
      fir.loadFromFile(argv[i + 1]);
      fir.setNormalization(true);
      Effect<float> effect;
      effect.sampleRate = SAMPLERATE;
      effect.effectType = Effect<float>::Type::Fir;
      config.effectFIR = effect;
      config.effectFIR->addFIR(fir);
    } else if (arg == "--sustain" && i + 1 < argc) {
      config.adsr.sustain_level =
          std::stof(argv[i + 1]) * config.adsr.amplitude;
    } else if (arg == "--volume" && i + 1 < argc) {
      config.volume = std::stof(argv[i + 1]);
    } else if (arg == "--duration" && i + 1 < argc) {
      config.duration = std::stof(argv[i + 1]);
      config.adsr.setLength(
          static_cast<int>(SAMPLERATE * config.duration * config.adsr.quantas));
    } else if (arg == "-h" || arg == "--help") {
      printHelp(argv[0]);
      return -1;
    }
  }
  // make sure there is always tremolo and vibrato
  ensureTremolo(6.0, 0.0);
  ensureVibrato(6.0, 0.0);
  return 0;
}

void start_http_server(KeyboardStream *kbs) {
  const char *options[] = {"document_root", "public", "listening_ports", "8080",
                           nullptr};

  static struct mg_callbacks callbacks = {};
  static struct mg_context *ctx = mg_start(&callbacks, nullptr, options);
  if (ctx == nullptr) {
    std::fprintf(
        stderr, "[HTTP] ERROR: Failed to start CivetWeb server on port 8080\n");
    return;
  }

  mg_set_request_handler(ctx, "/api/oscillators", oscillator_api_handler, kbs);
  mg_set_request_handler(ctx, "/api/input/push", input_push_handler, kbs);
  mg_set_request_handler(ctx, "/api/input/release", input_release_handler, kbs);
  mg_set_request_handler(ctx, "/api/config", config_api_handler, kbs);
  mg_set_request_handler(ctx, "/api/presets", presets_api_handler, kbs);

  printw("\nHttp server for synth configuration running on port 8080, "
         "http://localhost:8080\n");

  // Run forever — or you can add shutdown logic
  while (true)
    std::this_thread::sleep_for(std::chrono::hours(1));

  mg_stop(ctx);
}

int main(int argc, char *argv[]) {
  float duration = 0.1f;
  short amplitude = 32767;
  int maxPolyphony = 50;
  ADSR adsr =
      ADSR(amplitude, 1, 1, 3, 3, 0.8, static_cast<int>(SAMPLERATE * duration));
  KeyboardStream stream(SAMPLERATE);
  bool running = true;
  int port = 8080;
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_AudioSpec desired, obtained;
  SDL_zero(desired);
  desired.freq = SAMPLERATE;
  desired.format = AUDIO_F32SYS;
  desired.channels = 1;
  desired.samples = BUFFER_SIZE;
  desired.callback = audioCallback;
  desired.userdata = &stream;

  if (SDL_OpenAudio(&desired, &obtained) < 0) {
    std::cerr << "SDL_OpenAudio failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  // stream.startKeypressWatchdog();

  SDL_PauseAudio(0); // Start audio

  int rankIndex = 0;
  std::vector<Sound::Rank<float>::Preset> presets = {
      Sound::Rank<float>::Preset::Sine,
      Sound::Rank<float>::Preset::Saw,
      Sound::Rank<float>::Preset::Square,
      Sound::Rank<float>::Preset::Triangular,
      Sound::Rank<float>::Preset::SuperSaw,
      Sound::Rank<float>::Preset::FatTriangle,
      Sound::Rank<float>::Preset::PulseSquare,
      Sound::Rank<float>::Preset::SineSawDrone,
      Sound::Rank<float>::Preset::SuperSawWithSub,
      Sound::Rank<float>::Preset::GlitchMix,
      Sound::Rank<float>::Preset::OrganTone,
      Sound::Rank<float>::Preset::LushPad,
      Sound::Rank<float>::Preset::RetroLead,
      Sound::Rank<float>::Preset::BassGrowl,
      Sound::Rank<float>::Preset::AmbientDrone,
      Sound::Rank<float>::Preset::SynthStab,
      Sound::Rank<float>::Preset::GlassBells};

  PlayConfig config;
  config.adsr = adsr;
  config.rankPreset = presets[rankIndex];
  int c = parseArguments(argc, argv, config);
  if (c < 0) {
    return 0;
  } else if (c > 0) {
    return 1;
  }

  printf("Processing buffers... preparing sound..\n");

  if (config.waveFile.size() > 0) {
    stream.loadSoundMap(config.waveFile);
    config.waveForm = Sound::WaveForm::WaveFile;
  }
  stream.setLoaderFunc(loaderFunc);
  stream.setVolume(config.volume);
  std::vector<Effect<float>> effects;
  // Always have an echo effect included
  Effect<float> echo;
  echo.effectType = Effect<float>::Type::Echo;
  echo.config = config.effectEcho;
  effects.push_back(echo);
  printf("Adding echo with mix %f\n", config.effectEcho.mix);
  // Conditionally applied effects
  if (config.effectFIR) {
    effects.push_back(*config.effectFIR);
  }
  if (config.effectChorus) {
    effects.push_back(*config.effectChorus);
  }
  if (config.effectIIR) {
    effects.push_back(*config.effectIIR);
  }
  if (config.effectVibrato) {
    effects.push_back(*config.effectVibrato);
  }
  if (config.effectTremolo) {
    effects.push_back(*config.effectTremolo);
  }
  auto start = std::chrono::high_resolution_clock::now();
  stream.prepareSound(SAMPLERATE, config.adsr, effects);
  auto end = std::chrono::high_resolution_clock::now();
  auto prepTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();
  printf("Keyboard preparation time: %0.4f seconds",
         static_cast<double>(prepTime) / 1000.0);
  printf("\nSound OK!\n");
  initscr();            // Initialize the library
  cbreak();             // Line buffering disabled
  keypad(stdscr, TRUE); // Enable special keys
  noecho();             // Don't show the key being pressed
  scrollok(stdscr, TRUE);

  std::thread http_thread([&stream]() { start_http_server(&stream); });
  http_thread.detach(); // runs independently, main thread continues

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

    // 2) Build notesMap: startSample → vector<(noteKey, duration)>
    std::map<int, std::vector<std::pair<std::string, float>>> notesMap;
    for (int track = 0; track < midiFile.getTrackCount(); ++track) {
      int evCount = midiFile[track].size();
      for (int evIndex = 0; evIndex < evCount; ++evIndex) {
        auto &ev = midiFile[track][evIndex];
        if (!ev.isNoteOn())
          continue;
        int note = ev[1];
        float freq = noteToFreq(note);
        auto key = notes::getClosestNote(freq);
        float dur = ev.getDurationInSeconds();
        int startS = int(ev.seconds * SAMPLERATE);
        notesMap[startS].emplace_back(key, dur);
      }
    }

    // 4) Timing constants
    const double usPerSample = 1'000'000.0 / SAMPLERATE;

    auto startTimePoint =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

    // 6) Thread A: schedule registerNote()
    std::thread registerThread([&]() {
      for (auto const &[startSample, vec] : notesMap) {
        auto when = startTimePoint + std::chrono::microseconds(
                                         (long)(startSample * usPerSample));
        std::this_thread::sleep_until(when);

        for (auto const &p : vec) {
          stream.registerNote(p.first);
        }
      }
    });

    // 7) Thread B: schedule registerNoteRelease()
    std::thread releaseThread([&]() {
      for (auto const &[startSample, vec] : notesMap) {
        for (auto const &p : vec) {
          long offsetUs = long(startSample * usPerSample + p.second * 1e6);
          auto when = startTimePoint + std::chrono::microseconds(offsetUs);
          std::this_thread::sleep_until(when);
          stream.registerNoteRelease(p.first);
        }
      }
    });

    // 9) Wait for both to finish
    registerThread.join();
    releaseThread.join();

  } else {

    config.printConfig();
    stream.printInstructions();
    refresh();

    SDL_Window *window =
        SDL_CreateWindow("Keyboard Synth", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 100, 100, SDL_WINDOW_SHOWN);
    if (!window) {
      std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
      SDL_CloseAudio();
      SDL_Quit();
      endwin();
      return 1;
    }
    refresh();

    SDL_Event event;
    bool running = true;

    while (running) {
      while (SDL_PollEvent(&event)) {
        switch (event.type) {

        case SDL_QUIT:
          running = false;
          break;

        case SDL_KEYDOWN:
          if (!event.key.repeat) { // ignore key repeats
            int ch = event.key.keysym.sym;
            SDL_Keymod mod = SDL_GetModState();

            if (ch == SDLK_d && (mod & KMOD_CTRL)) {
              // CTRL+D to shutdown
              printf("CTRL-D - shutting down\n");
              running = false;
              break;
            }

            stream.registerButtonPress(ch);

            if (ch == SDLK_o || ch == SDLK_p) {
              if (mod & KMOD_SHIFT) {
                // 'O' or 'P' (capitalized)
                stream.teardown();
                printf("Updating the keyboard...\n");

                if (ch == SDLK_p) {
                  rankIndex = (rankIndex + 1) % presets.size();
                } else { // ch == SDLK_o
                  rankIndex = (rankIndex + presets.size() - 1) % presets.size();
                }

                stream.prepareSound(SAMPLERATE, config.adsr, effects);

                config.rankPreset = presets[rankIndex];
                config.printConfig();
                stream.printInstructions();
                printf(
                    "Updated to new preset %s\n",
                    Sound::Rank<float>::presetStr(presets[rankIndex]).c_str());
              } else {
                // plain 'o' or 'p' (lowercase)
                config.printConfig();
                stream.printInstructions();
              }
            }
          }
          break;

        case SDL_KEYUP: {
          int ch = event.key.keysym.sym;
          stream.registerButtonRelease(ch);
        } break;
        }
      }

      // Optionally you could add SDL_Delay(1); to not burn CPU
    }

    SDL_DestroyWindow(window);
  }

  endwin(); // End curses mode
  SDL_CloseAudio();
  SDL_Quit();

  return 0;
}
