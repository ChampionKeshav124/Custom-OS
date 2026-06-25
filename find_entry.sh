#!/bin/bash
# Find all child windows of the Aether Pilot window and try to send Return to the entry
export DISPLAY=:0

# Find the Aether Pilot X11 window
WID=$(xdotool search --name "AETHER PILOT" 2>/dev/null | head -1)
echo "Main Window ID: $WID (0x$(printf '%x' $WID))"

# List all child windows using xwininfo
echo "=== Child Windows ==="
xwininfo -id $WID -children 2>/dev/null | grep "child" | head -30

echo ""
echo "=== All descendants ==="
# Use xlsclients or xprop to find the entry widget
xdotool search --class "Tk" 2>/dev/null | head -20

# Try using wmctrl to find windows
echo ""
echo "=== wmctrl windows ==="
wmctrl -l 2>/dev/null | head -20

# Try to find the actual Tkinter entry child
echo ""
echo "=== xwininfo tree ==="
xwininfo -root -tree 2>/dev/null | grep -A2 "AETHER" | head -20
