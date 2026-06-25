#!/bin/bash
# Focus window and simulate Return key cleanly
export DISPLAY=:0

WID=$(xdotool search --name "AETHER PILOT" | head -1)
echo "Found window ID: $WID"
if [ -z "$WID" ]; then
    echo "Window not found"
    exit 1
fi

xdotool windowactivate --sync $WID
sleep 0.5
xdotool key Return
echo "Return key sent"
