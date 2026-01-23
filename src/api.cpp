#include "api.hpp"
#include "keyboardstream.hpp"
#include <json.hpp>

using json = nlohmann::json;

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

static void send_json(struct mg_connection *conn, const json &data,
                      int status = 200) {
  std::string body = data.dump();
  mg_printf(conn,
            "HTTP/1.1 %d OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "\r\n%s",
            status, body.size(), body.c_str());
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
    std::optional<std::reference_wrapper<Piper<float>>> reverbMixConf =
        std::nullopt;
    std::optional<
        std::reference_wrapper<Effect<float>::PhaseDistortionSinConfig>>
        phaseDistSinConf = std::nullopt;
    std::optional<std::reference_wrapper<Effect<float>::GainDistHardClipConfig>>
        gainDistConf = std::nullopt;
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
      if (auto reverbMix = std::get_if<Piper<float>>(&kbs->effects[e].config)) {
        reverbMixConf = std::ref(*reverbMix);
      }
      if (auto phaseDist = std::get_if<Effect<float>::PhaseDistortionSinConfig>(
              &kbs->effects[e].config)) {
        phaseDistSinConf = std::ref(*phaseDist);
      }
      if (auto gainDist = std::get_if<Effect<float>::GainDistHardClipConfig>(
              &kbs->effects[e].config)) {
        gainDistConf = std::ref(*gainDist);
      }
    }
    json response;
    if (echoConf && vibConf && tremConf && reverbMixConf && phaseDistSinConf &&
        gainDistConf) {
      response = {{"gain", kbs->gain},
                  {"adsr",
                   {{"attack", kbs->adsr.qadsr[0]},
                    {"decay", kbs->adsr.qadsr[1]},
                    {"sustain", kbs->adsr.qadsr[2]},
                    {"release", kbs->adsr.qadsr[3]}}},
                  {"tuning", notes::tuning_to_string(kbs->tuning)},
                  {"echo",
                   {{"rate", echoConf->get().getRate()},
                    {"feedback", echoConf->get().getFeedback()},
                    {"mix", echoConf->get().getMix()},
                    {"sampleRate", echoConf->get().getSampleRate()}}},
                  {"phaseDist", {{"depth", phaseDistSinConf->get().depth}}},
                  {"gainDist", {{"gain", gainDistConf->get().gain}}},
                  {"tremolo",
                   {{"depth", tremConf->get().depth},
                    {"frequency", tremConf->get().frequency}}},
                  {"reverb",
                   {{"dry", reverbMixConf->get().mix[1]},
                    {"wet", reverbMixConf->get().mix[0]}}},
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
      std::optional<std::reference_wrapper<Piper<float>>> reverbMixConf =
          std::nullopt;
      std::optional<
          std::reference_wrapper<Effect<float>::PhaseDistortionSinConfig>>
          phaseDistConf = std::nullopt;
      std::optional<
          std::reference_wrapper<Effect<float>::GainDistHardClipConfig>>
          gainDistConf = std::nullopt;
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
        if (auto reverbMix =
                std::get_if<Piper<float>>(&kbs->effects[e].config)) {
          reverbMixConf = std::ref(*reverbMix);
        }
        if (auto phaseDist =
                std::get_if<Effect<float>::PhaseDistortionSinConfig>(
                    &kbs->effects[e].config)) {
          phaseDistConf = std::ref(*phaseDist);
        }
        if (auto gainDist = std::get_if<Effect<float>::GainDistHardClipConfig>(
                &kbs->effects[e].config)) {
          gainDistConf = std::ref(*gainDist);
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

      if (phaseDistConf) {
        if (body.contains("phaseDist") && body["phaseDist"].is_object()) {
          json phaseDist = body["phaseDist"];
          if (phaseDist.contains("depth"))
            phaseDistConf->get().depth = phaseDist["depth"];
        }
      }

      if (gainDistConf) {
        if (body.contains("gainDist") && body["gainDist"].is_object()) {
          json gainDist = body["gainDist"];
          if (gainDist.contains("gain"))
            gainDistConf->get().gain = gainDist["gain"];
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

      if (reverbMixConf && body.contains("reverb") &&
          body["reverb"].is_object()) {
        json reverb = body["reverb"];
        if (reverb.contains("wet")) {
          reverbMixConf->get().mix[0] = reverb["wet"];
        }
        if (reverb.contains("dry")) {
          reverbMixConf->get().mix[1] = reverb["dry"];
        }
      } else {
        printf("Nope, no reverb\n");
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

// --- Recorder API Handler ---
// Supports:
//   GET  -> return current looper state
//   POST -> modify recorder (record, stop, set, clear)
int recorder_handler(struct mg_connection *conn, void *cbdata) {
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);
  if (!kbs) {
    mg_printf(
        conn,
        "HTTP/1.1 500 Internal Server Error\r\n\r\nMissing KeyboardStream");
    return 500;
  }

  Looper &looper = kbs->getLooper();

  const struct mg_request_info *req_info = mg_get_request_info(conn);

  // -----------------------------------------
  // HANDLE GET
  // -----------------------------------------
  if (strcmp(req_info->request_method, "GET") == 0) {
    json resp;
    resp["track"] = looper.getActiveTrack();
    resp["bpm"] = looper.getBPM();
    resp["metronome"] = looper.isMetronomeEnabled() ? "on" : "off";
    resp["recording"] = looper.isRecording();
    send_json(conn, resp);
    return 200;
  }

  // -----------------------------------------
  // HANDLE POST
  // -----------------------------------------
  char buffer[256];
  int len = mg_read(conn, buffer, sizeof(buffer) - 1);
  if (len <= 0) {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nEmpty body");
    return 400;
  }
  buffer[len] = '\0';

  try {
    json body = json::parse(buffer);

    if (!body.contains("action") || !body["action"].is_string()) {
      mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nMissing 'action'");
      return 400;
    }

    std::string action = body["action"];

    if (action == "record") {
      looper.setRecording(true);
      send_json(conn, {{"status", "ok"}, {"message", "Recording started"}});
      return 200;

    } else if (action == "stop") {
      looper.setRecording(false);
      send_json(conn, {{"status", "ok"}, {"message", "Recording stopped"}});
      return 200;

    } else if (action == "set") {
      if (!body.contains("track") || !body["track"].is_number_integer()) {
        mg_printf(conn,
                  "HTTP/1.1 400 Bad Request\r\n\r\nMissing or invalid 'track'");
        return 400;
      }
      if (!body.contains("bpm") || !body["bpm"].is_number()) {
        mg_printf(conn,
                  "HTTP/1.1 400 Bad Request\r\n\r\nMissing or invalid 'bpm'");
        return 400;
      }

      int track = body["track"];
      float bpm = body["bpm"];

      looper.setActiveTrack(track);
      looper.setBPM(bpm);

      if (body.contains("metronome") && body["metronome"].is_string()) {
        std::string metro = body["metronome"];
        bool enable = (metro == "on" || metro == "ON" || metro == "true");
        looper.enableMetronome(enable);
      }

      send_json(conn, {{"status", "ok"}, {"message", "Track and BPM updated"}});
      return 200;

    } else if (action == "clear") {
      if (!body.contains("track") || !body["track"].is_number_integer()) {
        mg_printf(conn,
                  "HTTP/1.1 400 Bad Request\r\n\r\nMissing or invalid 'track'");
        return 400;
      }
      int track = body["track"];
      looper.clearTrack(track);
      send_json(conn, {{"status", "ok"}, {"message", "Track cleared"}});
      return 200;

    } else {
      mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nUnknown action");
      return 400;
    }

  } catch (const std::exception &e) {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nInvalid JSON: %s",
              e.what());
    return 400;
  } catch (...) {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nUnknown error");
    return 400;
  }
}

// --- Waveform API Handler ---
// GET /api/waveform?id=0&samples=512
// Returns a single cycle of the oscillator waveform as JSON array
int waveform_api_handler(struct mg_connection *conn, void *cbdata) {
  const struct mg_request_info *req_info = mg_get_request_info(conn);
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);
  
  if (std::string(req_info->request_method) != "GET") {
    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
    return 405;
  }

  // Parse query parameters
  const char *query = req_info->query_string;
  if (!query) {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nMissing query parameters");
    return 400;
  }

  char id_str[32] = {0};
  char samples_str[32] = {0};
  mg_get_var(query, strlen(query), "id", id_str, sizeof(id_str));
  mg_get_var(query, strlen(query), "samples", samples_str, sizeof(samples_str));

  if (id_str[0] == '\0') {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n\r\nMissing 'id' parameter");
    return 400;
  }

  int id = std::atoi(id_str);
  int num_samples = samples_str[0] ? std::atoi(samples_str) : 512;
  
  if (num_samples <= 0 || num_samples > 4096) {
    num_samples = 512;
  }

  kbs->lock();

  if (id < 0 || id >= (int)kbs->synth.size()) {
    mg_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\nInvalid oscillator ID");
    kbs->unlock();
    return 404;
  }

  auto &osc = kbs->synth[id];
  
  // Calculate the actual frequency for this oscillator considering octave and detune
  const float base_freq = 440.0f;
  const float octave_multiplier = pow(2.0f, osc.octave);
  const float detune_multiplier = pow(2.0f, osc.detune / 1200.0f);
  const float actual_freq = base_freq * octave_multiplier * detune_multiplier;
  
  const int sample_rate = osc.sampleRate;
  const float samples_per_cycle = sample_rate / actual_freq;
  
  json waveform_data = json::array();
  
  // Create a temporary rank for visualization (no ADSR envelope)
  Sound::Rank<float> temp_rank = Sound::Rank<float>::fromPreset(
      osc.sound, actual_freq, (int)samples_per_cycle, sample_rate);
  
  // Generate samples for one complete cycle
  for (int i = 0; i < num_samples; i++) {
    float t = (float)i / num_samples * samples_per_cycle;
    float sample = temp_rank.generateRankSampleIndex((int)t);
    waveform_data.push_back(sample);
  }

  json response = {
    {"id", id},
    {"samples", num_samples},
    {"waveform", waveform_data},
    {"octave", osc.octave},
    {"detune", osc.detune},
    {"frequency", actual_freq}
  };

  kbs->unlock();

  std::string body = response.dump();
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\n\r\n%s",
            body.size(), body.c_str());
  
  return 200;
}

// --- Combined Waveform API Handler ---
// GET /api/waveform/combined?samples=512
// Returns a single cycle of all oscillators combined (weighted by volume)
int waveform_combined_api_handler(struct mg_connection *conn, void *cbdata) {
  const struct mg_request_info *req_info = mg_get_request_info(conn);
  KeyboardStream *kbs = static_cast<KeyboardStream *>(cbdata);
  
  if (std::string(req_info->request_method) != "GET") {
    mg_printf(conn, "HTTP/1.1 405 Method Not Allowed\r\n\r\n");
    return 405;
  }

  // Parse query parameters
  const char *query = req_info->query_string;
  char samples_str[32] = {0};
  if (query) {
    mg_get_var(query, strlen(query), "samples", samples_str, sizeof(samples_str));
  }

  int num_samples = samples_str[0] ? std::atoi(samples_str) : 512;
  
  if (num_samples <= 0 || num_samples > 4096) {
    num_samples = 512;
  }

  kbs->lock();

  // Find the lowest octave to determine the base frequency
  int min_octave = 0;
  bool has_active_osc = false;
  for (size_t osc_idx = 0; osc_idx < kbs->synth.size(); osc_idx++) {
    auto &osc = kbs->synth[osc_idx];
    if (osc.volume > 0.0f) {
      if (!has_active_osc || osc.octave < min_octave) {
        min_octave = osc.octave;
        has_active_osc = true;
      }
    }
  }

  // Reference frequency adjusted for the lowest octave
  // Base frequency is 440 Hz (A4), each octave doubles/halves the frequency
  const float base_freq = 440.0f;
  const float reference_freq = base_freq * pow(2.0f, min_octave);
  
  json waveform_data = json::array();
  json oscillator_info = json::array();
  
  // Initialize combined waveform array
  std::vector<float> combined_waveform(num_samples, 0.0f);
  
  // Generate waveform for each oscillator and sum them
  for (size_t osc_idx = 0; osc_idx < kbs->synth.size(); osc_idx++) {
    auto &osc = kbs->synth[osc_idx];
    
    // Skip oscillators with zero volume
    if (osc.volume == 0.0f) {
      oscillator_info.push_back({
        {"id", osc_idx},
        {"volume", 0.0f},
        {"octave", osc.octave},
        {"detune", osc.detune},
        {"active", false}
      });
      continue;
    }
    
    const int sample_rate = osc.sampleRate;
    
    // Calculate the actual frequency for this oscillator considering octave and detune
    // Octave: each octave up doubles the frequency
    // Detune: cents, where 100 cents = 1 semitone, 1200 cents = 1 octave
    const float octave_multiplier = pow(2.0f, osc.octave);
    const float detune_multiplier = pow(2.0f, osc.detune / 1200.0f);
    const float osc_freq = base_freq * octave_multiplier * detune_multiplier;
    
    // Calculate how many cycles this oscillator will complete in one cycle of the reference
    const float freq_ratio = osc_freq / reference_freq;
    
    // Create a temporary rank for visualization at this oscillator's frequency
    Sound::Rank<float> temp_rank = Sound::Rank<float>::fromPreset(
        osc.sound, osc_freq, sample_rate, sample_rate);
    
    // Generate samples and add to combined waveform
    // We need to show how this oscillator's waveform looks over one reference cycle
    for (int i = 0; i < num_samples; i++) {
      // Calculate the phase (0 to freq_ratio) for this sample
      // If freq_ratio = 2, we go through 2 complete cycles
      float normalized_phase = (float)i / (num_samples - 1); // 0 to 1
      float phase_in_cycles = normalized_phase * freq_ratio;  // 0 to freq_ratio
      
      // Convert phase to sample index in the oscillator's waveform
      // One cycle = sample_rate / osc_freq samples
      float samples_per_cycle = sample_rate / osc_freq;
      float sample_index = fmod(phase_in_cycles * samples_per_cycle, samples_per_cycle);
      
      float sample = temp_rank.generateRankSampleIndex((int)sample_index);
      combined_waveform[i] += sample * osc.volume;
    }
    
    oscillator_info.push_back({
      {"id", osc_idx},
      {"volume", osc.volume},
      {"octave", osc.octave},
      {"detune", osc.detune},
      {"sound", Sound::Rank<float>::presetStr(osc.sound)},
      {"active", true},
      {"frequency", osc_freq}
    });
  }
  
  // Convert combined waveform to JSON
  for (float sample : combined_waveform) {
    waveform_data.push_back(sample);
  }

  json response = {
    {"samples", num_samples},
    {"waveform", waveform_data},
    {"oscillators", oscillator_info},
    {"reference_frequency", reference_freq},
    {"base_octave", min_octave}
  };

  kbs->unlock();

  std::string body = response.dump();
  mg_printf(conn,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Content-Length: %zu\r\n\r\n%s",
            body.size(), body.c_str());
  
  return 200;
}
