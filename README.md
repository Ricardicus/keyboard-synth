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

```bash
./keyboard -h
Usage: ./keyboard [flags]
flags:
   --form: form of sound [sine (default), triangular, saw, square]
   -e|--echo: Add an echo effect
   -r|--reverb [file]: Add a reverb effect based on IR response in this wav file
   --file [file]: Use .wav files for notes with this mapping as provided in this file
   --volume [float]: Set the volume knob (default 1.0)

./keyboard compiled Sep  5 2023 18:45:28
```

## Map keys to wave files

```bash
./build/keyboard --file media/notes.json
```

Inspect the media/notes.json file to see how it is structured.
I have included already sounds from a piano found at https://theremin.music.uiowa.edu/MISpiano.html
by the University of Iowa. 

## Map IR to wave files

Include your own impulse responses to create reverb effects.
I have included the impulse response fro Kalvträsks kyrka (church in Sweden) uploaded by
someone here: https://familjenpalo.se/vpo/kalvtrask/ .

You can test this reverb on the piano like this:

```bash
./build/keyboard --file media/notes.json -r media/ir/KalvtraskStereo16bps-44100.wav
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

