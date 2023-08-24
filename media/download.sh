#!/bin/bash

# Base URL
BASE_URL="https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/piano"

# Notes array
NOTES=(C C# D D# E F F# G G# A A# B)

# Loop through octaves from 0 to 8
for OCTAVE in {0..8}; do
    for NOTE in "${NOTES[@]}"; do
        # Convert '#' to 'b' naming convention
        FILE_NOTE=$(echo "$NOTE" | sed 's/#/b/')

        # Construct the URL
        URL="$BASE_URL/Piano.pp.$FILE_NOTE$OCTAVE.aiff"

        # Download the file
        echo "Downloading $URL..."
        wget "$URL"
        
        # Break after C8
        if [ "$NOTE" == "C" ] && [ "$OCTAVE" == "8" ]; then
            break 2
        fi
    done
done

echo "Download completed."

