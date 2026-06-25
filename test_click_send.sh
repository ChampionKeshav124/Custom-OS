#!/bin/bash
# Focus window and click the send button
export DISPLAY=:0

WID=$(xdotool search --name "AETHER PILOT" | head -1)
echo "Found window ID: $WID"
if [ -z "$WID" ]; then
    echo "Window not found"
    exit 1
fi

eval "$(xdotool getwindowgeometry --shell $WID)"
echo "Window at X=$X Y=$Y W=$WIDTH H=$HEIGHT"

# Send button is on the right, ~25px from right edge and ~25px from bottom edge
BTN_X=$((X + WIDTH - 25))
BTN_Y=$((Y + HEIGHT - 25))
echo "Clicking send button at: $BTN_X,$BTN_Y"

xdotool mousemove --sync $BTN_X $BTN_Y
sleep 0.2
xdotool click 1
echo "Click sent"
