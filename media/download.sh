#!/bin/bash

# Base URL
BASE_URL="https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/piano"

# Notes array
NOTES=(A Bb B C Dd D E F Gb G Ab) 

# Loop through octaves from 0 to 8
for OCTAVE in {0..8}; do
    for NOTE in "${NOTES[@]}"; do
        # Convert '#' to 'b' naming convention
        FILE_NOTE=$(echo "$NOTE")

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

# Convert aiff to wave, raise volume 25 db
for file in *.aiff; do                            
    ffmpeg -i "$file" "${file%.aiff}.wav"                     
done
for file in Piano.pp.*; do                      
    ffmpeg -y -i "$file" -filter:a "volume=25dB" "louder_$file"
done
for f in louder_*; do mv "$f" "${f#louder_}"; done
rm *.aiff
