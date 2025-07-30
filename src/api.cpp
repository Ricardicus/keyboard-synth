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
    }
    json response;
    if (echoConf && vibConf && tremConf && reverbMixConf) {
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
