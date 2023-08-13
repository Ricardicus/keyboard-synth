# Keyboard synth

A little experimentation with OpenAL and audio signal processing in general lead me to this
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

Now you can press the keys a, s, d, f, g, h, j, k, l to make some sound.
But this is just the beginning. I want to add so that you can change octave and
have multiple rows also.

# Dependencies

You need OpenAL and ncurses installed.

````bash
# On OSX
brew install openal-soft
brew install ncurses
# On Debiand
sudo apt-get install libopenal-dev
sudo apt-get install libncurses5-dev
```

