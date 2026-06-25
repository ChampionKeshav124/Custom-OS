#!/bin/bash
# Deploy updated copilot.py from shared folder and restart it
set -e

# Kill any running copilot instances
pkill -f "copilot.py" 2>/dev/null || true
sleep 1

# Copy the new version
cp /media/sf_AetherOS/copilot_new.py /home/aether/copilot/copilot.py
echo "[deploy] copilot.py updated successfully"

# Restart copilot in background
export DISPLAY=:0
export HOME=/home/aether
export USER=aether
export XAUTHORITY=/home/aether/.Xauthority
nohup python3 -u /home/aether/copilot/copilot.py > /home/aether/copilot.log 2>&1 &
echo "[deploy] copilot restarted with PID $!"
