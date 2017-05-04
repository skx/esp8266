#!/bin/sh
#
# Interrupt the current music, if any, and play our alarm.
#
# Once the alarm has played then clear the playlist.
#
# TODO: Look at mpdtoys for saving/loading state, to avoid
# interfering with any custom playlist.
#
#

echo "Button pressed at $(date)"

# Unmute the audio, if muted
amixer set Master unmute
amixer -q -D pulse sset Master unmute

# Stop the current playlist
mpc stop

# Disable random/repeat
mpc random off
mpc repeat off

# Set the volume to be a decent level
mpc volume 55

# Clear the current-playlist
mpc clear

# Add a single file.
mpc add file:///$(pwd)/alarm.mp3

# Play
mpc play


echo "Alarm Complete at $(date)"
