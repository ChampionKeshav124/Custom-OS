#!/usr/bin/env python3
import os
import shutil
import time
import subprocess

DESKTOP_DIR = "/home/aether/Desktop"
SEEN_FILE = "/home/aether/.config/aether-desktop-sync-seen.txt"

# Standard desktop entry directories to search
SEARCH_DIRS = [
    "/usr/share/applications",
    "/var/lib/snapd/desktop/applications",
    "/usr/local/share/applications",
    "/home/aether/.local/share/applications"
]

def load_seen():
    if not os.path.exists(SEEN_FILE):
        return set()
    try:
        with open(SEEN_FILE, "r") as f:
            return set(line.strip() for line in f if line.strip())
    except Exception:
        return set()

def save_seen(seen):
    try:
        os.makedirs(os.path.dirname(SEEN_FILE), exist_ok=True)
        with open(SEEN_FILE, "w") as f:
            for item in sorted(seen):
                f.write(item + "\n")
    except Exception as e:
        print(f"Error saving seen list: {e}")

def parse_desktop_file(filepath):
    """
    Parses a .desktop file and returns a dictionary of key-value pairs,
    or None if it's not a valid desktop entry.
    """
    try:
        entry = {}
        in_section = False
        with open(filepath, "r", errors="ignore") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if line == "[Desktop Entry]":
                    in_section = True
                    continue
                elif line.startswith("[") and line.endswith("]"):
                    in_section = False
                    continue
                
                if in_section and "=" in line:
                    parts = line.split("=", 1)
                    key = parts[0].strip()
                    val = parts[1].strip()
                    entry[key] = val
        return entry
    except Exception:
        return None

def is_user_facing_app(entry):
    """
    Returns True if the desktop entry is a user-facing Application.
    """
    if not entry:
        return False
    # Check Type (default is Application if omitted, but standard requires it)
    entry_type = entry.get("Type", "Application")
    if entry_type != "Application":
        return False
    
    # Check NoDisplay
    if entry.get("NoDisplay", "false").lower() == "true":
        return False
    
    # Check Hidden
    if entry.get("Hidden", "false").lower() == "true":
        return False
    
    # Check Name and Exec
    if not entry.get("Name") or not entry.get("Exec"):
        return False
        
    return True

def sync_desktop():
    seen = load_seen()
    new_seen = False
    
    # Ensure desktop directory exists
    os.makedirs(DESKTOP_DIR, exist_ok=True)
    
    for search_dir in SEARCH_DIRS:
        if not os.path.isdir(search_dir):
            continue
            
        try:
            for filename in os.listdir(search_dir):
                if not filename.endswith(".desktop"):
                    continue
                    
                filepath = os.path.join(search_dir, filename)
                
                # Check if we have already processed this file
                if filepath in seen:
                    continue
                    
                entry = parse_desktop_file(filepath)
                if not entry:
                    seen.add(filepath)
                    new_seen = True
                    continue
                    
                if is_user_facing_app(entry):
                    # Copy to Desktop
                    dest_path = os.path.join(DESKTOP_DIR, filename)
                    if not os.path.exists(dest_path):
                        try:
                            shutil.copy2(filepath, dest_path)
                            os.chmod(dest_path, 0o755)
                            # Correct ownership
                            subprocess.run(["chown", "aether:aether", dest_path], check=False)
                            print(f"Copied {filename} to Desktop")
                        except Exception as e:
                            print(f"Error copying {filename}: {e}")
                
                # Add to seen so we don't process it again (even if it wasn't copied)
                seen.add(filepath)
                new_seen = True
        except Exception as e:
            print(f"Error scanning directory {search_dir}: {e}")
            
    if new_seen:
        save_seen(seen)

def main():
    # Loop indefinitely scanning for new apps
    while True:
        sync_desktop()
        time.sleep(2)

if __name__ == "__main__":
    main()
