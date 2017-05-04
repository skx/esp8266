#!/bin/sh

# Unmute the audio, if muted
amixer set Master unmute
amixer -q -D pulse sset Master unmute

# Stop the current playlist
mpc stop

# Disable random/repeat
mpc random off
mpc repeat off

# Set the volume to be a decent level
mpc volume 35

# Clear the current-playlist
mpc clear

# Add a single file.
mpc add file:///$(pwd)/alarm.mp3

# Play
mpc play

