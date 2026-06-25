#!/bin/bash
# Better test: focus the window then type and press Enter
export DISPLAY=:0

# Find the Aether Pilot window
WID=$(xdotool search --name "AETHER PILOT" 2>/dev/null | head -1)
echo "Window ID: $WID"

if [ -z "$WID" ]; then
    echo "ERROR: Could not find AETHER PILOT window"
    exit 1
fi

# Raise and focus the window
xdotool windowraise $WID
xdotool windowfocus --sync $WID
sleep 0.5

# Get window geometry
eval "$(xdotool getwindowgeometry --shell $WID 2>/dev/null)"
echo "Window at X=$X Y=$Y W=$WIDTH H=$HEIGHT"

# Click directly on the entry area - it's ~45px from bottom edge (inside window)
ENTRY_X=$((X + WIDTH / 2))
ENTRY_Y=$((Y + HEIGHT - 25))
echo "Clicking entry at: $ENTRY_X,$ENTRY_Y"
xdotool mousemove --sync $ENTRY_X $ENTRY_Y
sleep 0.2
xdotool click 1
sleep 0.5

# Check what's focused
FOCUSED=$(xdotool getwindowfocus 2>/dev/null)
echo "Focused window: $FOCUSED (Aether: $WID)"

# Type test message
xdotool type --clearmodifiers --delay 50 "ping test"
sleep 0.5

# Press Enter
xdotool key --window $WID Return
sleep 0.5
echo "Sent"

# Check log
echo "--- Log ---"
tail -30 /home/aether/copilot.log 2>/dev/null || echo "(empty)"
