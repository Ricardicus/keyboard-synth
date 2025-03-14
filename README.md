# Keyboard synth

A little experimentation with OpenAL and audio signal processing in general led me to this
project idea. Play music with the computer keyboard.


# Build

```bash
mkdir build
cd build
cmake ..
make
```

# Run 

```
./keyboard
```

Now you can press the keys on the a -> l and z -> , keys. Sharp notes: w, e, t, y, u. 
Change octaves with o (down) and p (up). Echo effect added with argument -e.
Use -h or --help for more info.

In general, for more help use -h:

```text
./keyboard -h
Usage: ./keyboard [flags]
flags:
   --form: form of sound [sine (default), triangular, saw, supersaw,square]
   -e|--echo: Add an echo effect
   --chorus: Add a chorus effect
   -r|--reverb [file]: Add a reverb effect based on IR response in this wav file
   --notes [file]: Map notes to .wav files as mapped in this .json file
   --midi [file]: Play this MIDI (.mid) file
   --volume [float]: Set the volume knob (default 1.0)
   --duration [float]: Note duration in seconds (default 0.8)
   --adsr [int,int,int,int]: Set the ADSR quant intervals comma-separated (default 1,1,3,3)
   --sustain [float]: Set the sustain level [0,1] (default 0.8)

./keyboard compiled Mar 13 2025 21:37:39
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

This in ebabled with the support of the ![midifile project](https://github.com/craigsapp/midifile).
I have included an example midi-file of Bach under media/midi/bmv988.mid
taken from the ![jsbash.net webpage](http://www.jsbach.net/midi/midi_goldbergvariations.html)

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

