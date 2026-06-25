#!/usr/bin/env python3
"""
Direct test of send_message by using xdotool to inject keyboard event
to the correct child window widget.
"""
import os
import subprocess
import sys

os.environ['DISPLAY'] = ':0'
os.environ['HOME'] = '/home/aether'

# Find the main Tkinter window by PID
result = subprocess.run(['pgrep', '-f', 'copilot.py'], capture_output=True, text=True)
pids = result.stdout.strip().split('\n')
print(f"Copilot PIDs: {pids}")

for pid in pids:
    if pid:
        # Use xdotool to find all windows for this PID
        r = subprocess.run(['xdotool', 'search', '--pid', pid], 
                          capture_output=True, text=True, env=os.environ)
        windows = r.stdout.strip().split('\n')
        print(f"PID {pid} windows: {windows}")
        
        for wid in windows:
            if not wid:
                continue
            # Get window name
            r2 = subprocess.run(['xdotool', 'getwindowname', wid], 
                               capture_output=True, text=True, env=os.environ)
            print(f"  WID {wid}: {r2.stdout.strip()}")
