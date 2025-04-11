#!/bin/bash

# Compile the window manager
cc ~/CLionProjects/untitled/barewm.c -o barewm -lX11 -lXext

# Start Xephyr with specified resolution
DISPLAY=:0 Xephyr -br -ac -noreset -screen 2560x1440 :1 &

# Wait a moment for Xephyr to start
sleep 1

# Set the DISPLAY to the Xephyr screen
export DISPLAY=:1

# Start barewm and set background
./barewm

feh --bg-scale ~/Pictures/x60.jpg

# Print a message
echo "barewm running"
