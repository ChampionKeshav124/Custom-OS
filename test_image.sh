#!/bin/bash
# Focus window, type image generation command, and simulate Return key
export DISPLAY=:0

WID=$(xdotool search --name "AETHER PILOT" | head -1)
echo "Found window ID: $WID"
if [ -z "$WID" ]; then
    echo "Window not found"
    exit 1
fi

xdotool windowactivate --sync $WID
sleep 0.5

# Click the entry widget to focus it
eval "$(xdotool getwindowgeometry --shell $WID)"
ENTRY_X=$((X + WIDTH / 2))
ENTRY_Y=$((Y + HEIGHT - 25))
xdotool mousemove --sync $ENTRY_X $ENTRY_Y
xdotool click 1
sleep 0.2

# Type command
xdotool type --clearmodifiers --delay 50 "/image a holographic cyan circle"
sleep 0.5

# Press Return
xdotool key Return
echo "Image command sent"
