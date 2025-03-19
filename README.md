# Keyboard synth

A little experimentation with OpenAL and audio signal processing in general led me to this
project idea. Play music with the computer keyboard.


# Build

Clone the source tree and build using CMake

```bash
git clone https://github.com/Ricardicus/keyboard-synth.git
cd keyboard-synth
mkdir build
cmake -B build
cmake --build build
```

# Run 

```
./build/keyboard
```

Now you can press the keys on the a -> l and z -> , keys. Sharp notes: w, e, t, y, u. 
Change octaves with o (down) and p (up). Echo effect added with argument -e.
Use -h or --help for more info.

In general, for more help use -h:

```text
./build/keyboard -h
Usage: ./build/keyboard [flags]
flags:
   --form: form of sound [sine (default), triangular, saw, supersaw, 
           square, fattriangle, pulsesquare, sinesawdrone, supersawsub, 
           glitchmix, lushpad, retroLead, bassgrowl, ambientdrone,
           synthstab, glassbells]
   -e|--echo: Add an echo effect
   --chorus: Add a chorus effect with default settings
   --chorus_delay [float]: Set the chorus delay factor, default: 0.45
   --chorus_depth [float]: Set the chorus depth factor, in pitch cents, default: 3
   --chorus_voices[int]: Set the chorus voices, default: 3
   --vibrato: Add a vibrato effect with default settings
   --vibrato_depth [float]: Set the vibrato depth factor, default: 0.3
   --vibrato_frequency [float]: Set the vibrato frequency, in Herz  default: 6
   -r|--reverb [file]: Add a reverb effect based on IR response in this wav file
   --notes [file]: Map notes to .wav files as mapped in this .json file
   --midi [file]: Play this MIDI (.mid) file
   --volume [float]: Set the volume knob (default 1.0)
   --duration [float]: Note ADSR quanta duration in seconds (default 0.1)
   --adsr [int,int,int,int]: Set the ADSR quant intervals comma-separated (default 1,1,3,3)
   --sustain [float]: Set the sustain level [0,1] (default 0.8)
   --lowpass [float]: Set the lowpass filter cut off frequency in Hz
                   (default no low pass)
   --highpass [float]: Set the highpass filter cut off frequency in Hz
                (default no highpass)
   --parallelization [int]: Number of threads used in keyboard preparation default: 8
./build/keyboard compiled Mar 19 2025 22:26:18
```

## Map keys to wave files

```bash
./build/keyboard --notes media/notes.json
```

Inspect the media/notes.json file to see how it is structured.
I have included already sounds from a piano found at https://theremin.music.uiowa.edu/MISpiano.html
by the University of Iowa. 

## Map IR to wave files

Include your own impulse responses to create reverb effects.
I have included the impulse response from Kalvträsks kyrka (church in Sweden) uploaded by
someone named Lars here: https://familjenpalo.se/vpo/kalvtrask/ .

You can test this reverb on the piano like this:

```bash
./build/keyboard --notes media/notes.json --reverb media/ir/KalvtraskStereo16bps-44100.wav
```

## Play MIDI files

Instead of taking input from the keyboard, the keyboard can also be configured to
receive input events from MIDI files.

This in ebabled with the support of the [midifile project](https://github.com/craigsapp/midifile).
I have included an example midi-file of Bach under media/midi/bmv988.mid
taken from the [jsbash.net webpage](http://www.jsbach.net/midi/midi_goldbergvariations.html)

```bash
./build/keyboard --midi media/midi/bwv988.mid
```

# Dependencies

You need OpenAL and ncurses installed.

```bash
# On OSX
brew install openal-soft
brew install ncurses
# On Debiand
sudo apt-get install libopenal-dev
sudo apt-get install libncurses5-dev
```

# Todo

- API

