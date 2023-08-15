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
Change octaves with o (down) and p (up).

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

