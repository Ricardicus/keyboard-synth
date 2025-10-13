
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

#include <json.hpp>

using json = nlohmann::json;

#include "MidiFile.h"

#include "adsr.hpp"
#include "api.hpp"
#include "config.hpp"
#include "effect.hpp"
#include "keyboardstream.hpp"

// MIDI note number to frequency
float noteToFreq(int note) { return 440.0f * pow(2, (note - 69) / 12.0f); }

bool fileExists(const std::string &file) {
  return std::filesystem::exists(file);
}

void SDL2AudioCallback(void *userdata, Uint8 *stream, int len) {
  auto *ks = static_cast<KeyboardStream *>(userdata);
  float *streamBuf = reinterpret_cast<float *>(stream);
  int samples = len / sizeof(float);
  ks->lock();
  ks->fillBuffer(streamBuf, samples);
  ks->unlock();
}

void printHelp(char *argv0) {
  printf("Usage: %s [flags]\n", argv0);
  printf("flags:\n");
  printf("   -p|--port [int]: Port to host synth config tool on\n");
  printf("                    per default 8080, http://localhost:8080\n");
  printf("   -e|--echo: Add an echo effect\n");
  printf("   --reverb: Add a synthetic reverb effect\n");
  printf("   --chorus: Add a chorus effect with default settings\n");
  printf("   --chorus_delay [float]: Set the chorus delay factor, default: "
         "0.45\n");
  printf("   --chorus_depth [float]: Set the chorus depth factor, in pitch "
         "cents, default: "
         "3\n");
  printf("   --chorus_voices[int]: Set the chorus voices, default: 3\n");
  printf("   --vibrato: Add a vibrato effect with default settings\n");
  printf("   --vibrato-depth [float]: Set the vibrato depth factor, default: "
         "0.3\n");
  printf("   --vibrato-frequency [float]: Set the vibrato frequency, in Hertz "
         " default: 6\n");
  printf("   --phaseDist: Add a phase dist (sinus) with default setting\n");
  printf("   --gainDist: Add a gain dist (hard clip) with default settings\n");
  printf("   --tremolo: Add a tremolo effect with default settings\n");
  printf("   --tremolo-depth [float]: Set the tremolo depth factor [0-1], "
         "default: "
         "1.0\n");
  printf("   --tremolo-frequency [float]: Set the tremolo frequency, in Hertz "
         " default: 18\n");
  printf("   --notes [file]: Map notes to .wav files as mapped in this .json "
         "file\n");
  printf("   --midi [file]: Play this MIDI (.mid) file\n");
  printf("   --volume [float]: Set the volume knob (default 1.0)\n");
  printf("   --legato [float]: Set legato, and legato speed in milliseconds "
         "(default 500)\n");
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
         "preparation default: 8\n");
  printf("   --tuning [string]: Set the tuning used (equal | werckmeister3)\n");
  printf("   --looper: Activate a looper, will work based on metronome-bpm\n");
  printf("   --looper-bars: Set how many bars the looper will operate over "
         "(default 8)\n");
  printf("   --metronome: Activate the metronome\n");
  printf("   --metronome-bpm [int]: Set the metronome bpm (default: %d)\n",
         Config::instance().getMetronomeBPM());
  printf(
      "   --metronome-volume [float]: Set the metronome volume (default: %f)\n",
      Config::instance().getMetronomeVolume());
  printf(
      "   --metronome-low [string]: Set the metronome low sound to this wave "
      "file (needs to be of %d sample rate)\n",
      Config::instance().getSampleRate());
  printf("   --metronome-high [string]: Set the metronome high sound to this "
         "wave file (needs to be of %d sample rate)\n",
         Config::instance().getSampleRate());

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

int parseArguments(int argc, char *argv[], KeyboardStreamPlayConfig &config) {
  auto ensureVibrato = [&](float defFreq = 6.0f, float defDepth = 0.3f) {
    if (!config.effectVibrato) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::Vibrato;
      e.config = Effect<float>::VibratoConfig{defFreq, defDepth};
      e.sampleRate =
          Config::instance().getSampleRate(); // make sure SR is set once
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
      e.sampleRate =
          Config::instance().getSampleRate(); // make sure SR is set once
      config.effectChorus = e;
    }
  };
  auto ensureTremolo = [&](float defFreq = 5.0f, float defDepth = 0.5f) {
    if (!config.effectTremolo) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::Tremolo;
      e.config = Effect<float>::TremoloConfig{defFreq, defDepth};
      e.sampleRate = Config::instance().getSampleRate(); // set once
      config.effectTremolo = e;
    }
  };
  auto ensurePhaseDist = [&](float defDepth = 0.0f) {
    if (!config.effectPhaseDist) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::PhaseDistortionSin;
      e.config = Effect<float>::PhaseDistortionSinConfig{defDepth};
      e.sampleRate =
          Config::instance().getSampleRate(); // make sure SR is set once
      config.effectPhaseDist = e;
    }
  };
  auto ensureGainDist = [&](float defGain = 1.0f) {
    if (!config.effectGainDist) {
      Effect<float> e;
      e.effectType = Effect<float>::Type::GainDistHardClip;
      e.config = Effect<float>::GainDistHardClipConfig{defGain};
      e.sampleRate =
          Config::instance().getSampleRate(); // make sure SR is set once
      config.effectGainDist = e;
    }
  };

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "--port" && i + 1 < argc) {
      config.port = atoi(argv[i + 1]);
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
    } else if (arg == "--reverb") {
      config.effectReverb = true;
    } else if (arg == "--vibrato") { // enable with defaults
      ensureVibrato();               // only creates if missing
    } else if (arg == "--legato" && i + 1 < argc) {
      config.legatoSpeed = std::stof(argv[i + 1]); // change depth only
      printf("config.legatoSpeed: %f\n", *config.legatoSpeed);
    } else if (arg == "--vibrato-depth" && i + 1 < argc) {
      ensureVibrato(); // keep existing freq
      auto &v =
          std::get<Effect<float>::VibratoConfig>(config.effectVibrato->config);
      v.depth = std::stof(argv[i + 1]); // change depth only

    } else if (arg == "--vibrato-frequency" && i + 1 < argc) {
      ensureVibrato(); // keep existing depth
      auto &v =
          std::get<Effect<float>::VibratoConfig>(config.effectVibrato->config);
      v.frequency = std::stof(argv[i + 1]); // change frequency only
    } else if (arg == "--phaseDist") {      // enable with defaults
      ensurePhaseDist(0.3);
    } else if (arg == "--gainDist") { // enable with defaults
      ensureGainDist(2.0);
    } else if (arg == "--tremolo-depth" && i + 1 < argc) {
      ensurePhaseDist(std::stof(argv[i + 1]));
    } else if (arg == "--tremolo") { // enable with defaults
      ensureTremolo();
    } else if (arg == "--tremolo-depth" && i + 1 < argc) {
      ensureTremolo(); // keep existing freq
      auto &v =
          std::get<Effect<float>::TremoloConfig>(config.effectTremolo->config);
      v.depth = std::stof(argv[i + 1]);
    } else if (arg == "--tremolo-frequency" && i + 1 < argc) {
      ensureTremolo(); // keep existing depth
      auto &v =
          std::get<Effect<float>::TremoloConfig>(config.effectTremolo->config);
      v.frequency = std::stof(argv[i + 1]);
    } else if (arg == "--lowpass" && i + 1 < argc) {
      Effect<float> effect;
      effect.effectType = Effect<float>::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = Config::instance().getSampleRate();
      IIR<float> lowPass = IIRFilters::lowPass<float>(
          config.effectIIR->sampleRate, std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);
    } else if (arg == "--highpass" && i + 1 < argc) {
      Effect<float> effect;
      effect.effectType = Effect<float>::Type::Iir;
      config.effectIIR = effect;
      config.effectIIR->sampleRate = Config::instance().getSampleRate();
      IIR<float> lowPass = IIRFilters::highPass<float>(
          config.effectIIR->sampleRate, std::stof(argv[i + 1]));
      config.effectIIR->iirs.push_back(lowPass);

    } else if (arg == "--looper") {
      config.metronomeActive = true;
      config.looperActive = true;
    } else if (arg == "--looper-bars" && i + 1 < argc) {
      config.looperBars = std::stoi(argv[i + 1]);
      Config::instance().setNumBars(config.looperBars);
    } else if (arg == "--metronome") {
      config.metronomeActive = true;
    } else if (arg == "--metronome-volume" && i + 1 < argc) {
      Config::instance().setMetronomeVolume(std::stof(argv[i + 1]));
    } else if (arg == "--metronome-bpm" && i + 1 < argc) {
      Config::instance().setMetronomeBPM(std::stoi(argv[i + 1]));
    } else if (arg == "--metronome-low" && i + 1 < argc) {
      config.metronomeLow = std::string(argv[i + 1]);
    } else if (arg == "--metronome-high" && i + 1 < argc) {
      config.metronomeHigh = std::string(argv[i + 1]);
    } else if (arg == "--tuning") { // enable with defaults
      if (i + 1 < argc) {           // make sure there's a value after --tuning
        std::string tuningArg = argv[++i];
        if (tuningArg == "equal") {
          config.tuning = notes::TuningSystem::EqualTemperament;
        } else if (tuningArg == "werckmeister3") {
          config.tuning = notes::TuningSystem::WerckmeisterIII;
        } else {
          std::cerr << "Unknown tuning: " << tuningArg
                    << " (expected equal or werckmeister3)\n";
          return 1;
        }
      } else {
        std::cerr << "--tuning requires a value (equal | "
                     "werckmeister3)\n";
        return 1;
      }
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
      FIR fir(Config::instance().getSampleRate());
      fir.loadFromFile(argv[i + 1]);
      fir.setNormalization(true);
      Effect<float> effect;
      effect.sampleRate = Config::instance().getSampleRate();
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
          static_cast<int>(Config::instance().getSampleRate() *
                           config.duration * config.adsr.quantas));
    } else if (arg == "-h" || arg == "--help") {
      printHelp(argv[0]);
      return -1;
    }
  }
  // make sure there is always tremolo and vibrato
  ensureTremolo(6.0, 0.0);
  ensureVibrato(6.0, 0.0);
  ensurePhaseDist(0.0);
  ensureGainDist();
  return 0;
}

void start_http_server(KeyboardStream *kbs, int port) {
  char portStr[20];
  memset(portStr, 0, sizeof(portStr));
  snprintf(portStr, sizeof(portStr), "%d", port);
  const char *options[] = {"document_root", "public", "listening_ports",
                           portStr, nullptr};

  static struct mg_callbacks callbacks = {};
  static struct mg_context *ctx = mg_start(&callbacks, nullptr, options);
  if (ctx == nullptr) {
    std::fprintf(stderr,
                 "[HTTP] ERROR: Failed to start CivetWeb server on port %d\n",
                 port);
    return;
  }

  mg_set_request_handler(ctx, "/api/oscillators", oscillator_api_handler, kbs);
  mg_set_request_handler(ctx, "/api/input/push", input_push_handler, kbs);
  mg_set_request_handler(ctx, "/api/input/release", input_release_handler, kbs);
  mg_set_request_handler(ctx, "/api/config", config_api_handler, kbs);
  mg_set_request_handler(ctx, "/api/presets", presets_api_handler, kbs);
  mg_set_request_handler(ctx, "/api/recorder", recorder_handler, kbs);

  printw("\nHttp server for synth configuration running on port %d, "
         "http://localhost:%d\n",
         port, port);

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
      ADSR(amplitude, 1, 1, 3, 3, 0.8,
           static_cast<int>(Config::instance().getSampleRate() * duration));

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

  KeyboardStreamPlayConfig config;
  config.adsr = adsr;
  config.rankPreset = presets[rankIndex];
  int c = parseArguments(argc, argv, config);
  if (c < 0) {
    return 0;
  } else if (c > 0) {
    return 1;
  }

  KeyboardStream stream(Config::instance().getSampleRate(), config.tuning);
  bool running = true;
  int port = config.port;
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_AudioSpec desired, obtained;
  SDL_zero(desired);
  desired.freq = Config::instance().getSampleRate();
  desired.format = AUDIO_F32SYS;
  desired.channels = 1;
  desired.samples = Config::instance().getBufferSize();
  desired.callback = SDL2AudioCallback;
  desired.userdata = &stream;

  if (SDL_OpenAudio(&desired, &obtained) < 0) {
    std::cerr << "SDL_OpenAudio failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  // stream.startKeypressWatchdog();

  SDL_PauseAudio(0); // Start audio

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
  if (config.effectPhaseDist) {
    effects.push_back(*config.effectPhaseDist);
  }
  if (config.effectGainDist) {
    effects.push_back(*config.effectGainDist);
  }

  Effect<float> reverb;
  if (config.effectReverb) {
    reverb = PresetEffects::syntheticReverb(1.0, 0.7);
  } else {
    reverb = PresetEffects::syntheticReverb(1.0, 0.0);
  }
  effects.push_back(reverb);
  auto start = std::chrono::high_resolution_clock::now();
  stream.prepareSound(Config::instance().getSampleRate(), config.adsr, effects);
  if (config.legatoSpeed) {
    stream.setLegato(true, *config.legatoSpeed);
  }

  // Setup metronome in the keyboardstream looper
  Looper &looper = stream.getLooper();
  if (!looper.setMetronomeSampler(config.metronomeHigh, config.metronomeLow)) {
    printf("Failed to load metronome sounds:\n");
    printf("  metronome-low:  %s\n", config.metronomeLow.c_str());
    printf("  metronome-high: %s\n", config.metronomeHigh.c_str());
    printf("Are they are wave-files with sample rate: %d?\n",
           Config::instance().getSampleRate());
  }
  looper.setBPM(static_cast<float>(Config::instance().getMetronomeBPM()));
  looper.setMetronomeVolume(Config::instance().getMetronomeVolume());
  looper.setNumBars(config.looperBars);
  if (config.metronomeActive) {
    looper.enableMetronome(true);
  } else {
    looper.enableMetronome(false);
  }

  if (config.looperActive) {
    looper.setRecording(true);
  }

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

  std::thread http_thread(
      [&stream, port]() { start_http_server(&stream, port); });
  http_thread.detach(); // runs independently, main thread continues

  if (config.midiFile.size() > 0) {
    clear();
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
        auto key = notes::getClosestNote(freq, config.tuning);
        float dur = ev.getDurationInSeconds();
        int startS = int(ev.seconds * Config::instance().getSampleRate());
        notesMap[startS].emplace_back(key, dur);
      }
    }

    // 4) Timing constants
    const double usPerSample = 1'000'000.0 / Config::instance().getSampleRate();

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
    clear();
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

                stream.prepareSound(Config::instance().getSampleRate(),
                                    config.adsr, effects);

                config.rankPreset = presets[rankIndex];
                clear();
                config.printConfig();
                stream.printInstructions();
                printf(
                    "Updated to new preset %s\n",
                    Sound::Rank<float>::presetStr(presets[rankIndex]).c_str());
              } else {
                // plain 'o' or 'p' (lowercase)
                clear();
                config.printConfig();
                stream.printInstructions();
              }
            }
            if (ch == SDLK_SPACE || ch == '.' || ch == ',') {
              clear();
              config.printConfig();
              stream.printInstructions();
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
