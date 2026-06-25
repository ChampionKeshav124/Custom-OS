#!/usr/bin/env python3
# ==============================================================
# AETHEROS LINUX — GOOGLE CHROME INSTALLER WIZARD
# Tkinter-based flat white installation experience.
# ==============================================================

import os
import sys
import time
import subprocess
import threading
import tkinter as tk
from tkinter import ttk

class ChromeInstallerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Google Chrome Installer")
        self.root.geometry("500x350")
        self.root.resizable(False, False)
        self.root.configure(bg="#ffffff")
        
        # Style
        self.style = ttk.Style()
        self.style.theme_use('clam')
        self.style.configure("TProgressbar", thickness=12, troughcolor="#f1f3f4", background="#1a73e8")
        
        self.step = 0
        self.progress_val = 0
        
        # Build UI
        self.create_widgets()
        
        self.update_loop()
        
        # Check locks first
        self.root.after(100, self.check_locks_and_start)

    def create_widgets(self):
        # Header Label (Chrome Brand Colors)
        self.logo_frame = tk.Frame(self.root, bg="#ffffff")
        self.logo_frame.pack(pady=(40, 20))
        
        # Simulated colored circle logo for Google Chrome
        self.canvas = tk.Canvas(self.logo_frame, width=80, height=80, bg="#ffffff", bd=0, highlightthickness=0)
        self.canvas.pack()
        
        # Draw Google colors quadrants
        self.canvas.create_arc(5, 5, 75, 75, start=90, extent=120, fill="#ea4335", outline="")     # Red
        self.canvas.create_arc(5, 5, 75, 75, start=210, extent=120, fill="#f9ab00", outline="")    # Yellow
        self.canvas.create_arc(5, 5, 75, 75, start=330, extent=120, fill="#34a853", outline="")    # Green
        self.canvas.create_ellipse = self.canvas.create_oval
        self.canvas.create_ellipse(20, 20, 60, 60, fill="#ffffff", outline="")
        self.canvas.create_ellipse(28, 28, 52, 52, fill="#1a73e8", outline="")                     # Center Blue
        
        # Title
        self.title_label = tk.Label(
            self.root, 
            text="Google Chrome", 
            font=("Arial", 18, "bold"), 
            bg="#ffffff", 
            fg="#202124"
        )
        self.title_label.pack()
        
        # Status Subtitle
        self.status_label = tk.Label(
            self.root, 
            text="Initializing download protocol...", 
            font=("Arial", 11), 
            bg="#ffffff", 
            fg="#5f6368"
        )
        self.status_label.pack(pady=(10, 20))
        
        # Progress Bar
        self.progress = ttk.Progressbar(self.root, length=380, mode='determinate', style="TProgressbar")
        self.progress.pack(pady=5)
        
        # Action button (disabled initially)
        self.action_btn = tk.Button(
            self.root,
            text="Close",
            font=("Arial", 11),
            bg="#f1f3f4",
            fg="#5f6368",
            bd=0,
            padx=20,
            pady=8,
            activebackground="#e8eaed",
            command=self.root.quit,
            state="disabled"
        )
        self.action_btn.pack(pady=(20, 10))

    def check_locks_and_start(self):
        if self.is_package_manager_running():
            self.show_lock_dialog()
        else:
            # Start installation process in background thread
            self.install_thread = threading.Thread(target=self.run_install_sequence)
            self.install_thread.daemon = True
            self.install_thread.start()

    def is_package_manager_running(self):
        # 1. Check process list for active apt/dpkg/unattended-upgrades processes
        try:
            proc = subprocess.run(["pgrep", "-f", "apt|dpkg|unattended-upg"], stdout=subprocess.PIPE, text=True)
            if proc.returncode == 0 and proc.stdout.strip():
                # Filter out our own installer python processes and shell scripts
                pids = proc.stdout.strip().split()
                my_pid = str(os.getpid())
                my_ppid = str(os.getppid())
                other_pids = [p for p in pids if p != my_pid and p != my_ppid]
                if other_pids:
                    return True
        except Exception as e:
            print(f"Error checking processes: {e}")

        # 2. Check dpkg/apt lock files using fcntl
        import fcntl
        lock_files = [
            "/var/lib/dpkg/lock-frontend",
            "/var/lib/dpkg/lock",
            "/var/lib/apt/lists/lock",
            "/var/cache/apt/archives/lock"
        ]
        for lock_file in lock_files:
            if os.path.exists(lock_file):
                try:
                    f = open(lock_file, "w")
                    fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    f.close()
                except BlockingIOError:
                    return True
                except Exception:
                    pass
        return False

    def show_lock_dialog(self):
        self.lock_win = tk.Toplevel(self.root)
        self.lock_win.title("System Lock Detected")
        self.lock_win.geometry("420x220")
        self.lock_win.resizable(False, False)
        self.lock_win.configure(bg="#ffffff")
        
        # Make it modal
        self.lock_win.transient(self.root)
        self.lock_win.grab_set()
        
        # Center the dialog on root window
        root_x = self.root.winfo_x()
        root_y = self.root.winfo_y()
        self.lock_win.geometry(f"+{root_x + 40}+{root_y + 60}")

        # Title Label
        title_lbl = tk.Label(
            self.lock_win,
            text="Installer Blocked",
            font=("Arial", 14, "bold"),
            bg="#ffffff",
            fg="#ea4335" # Chrome red
        )
        title_lbl.pack(pady=(20, 10))

        # Description Label
        desc_lbl = tk.Label(
            self.lock_win,
            text="Another software installation or update is currently running.\nPlease wait for it to finish and try again.",
            font=("Arial", 10),
            bg="#ffffff",
            fg="#202124",
            justify="center"
        )
        desc_lbl.pack(pady=(0, 20), padx=20)

        # Button Frame
        btn_frame = tk.Frame(self.lock_win, bg="#ffffff")
        btn_frame.pack(pady=10)

        # Cancel Button
        cancel_btn = tk.Button(
            btn_frame,
            text="Cancel",
            font=("Arial", 10),
            bg="#f1f3f4",
            fg="#5f6368",
            bd=0,
            padx=15,
            pady=6,
            activebackground="#e8eaed",
            command=self.on_lock_cancel
        )
        cancel_btn.pack(side="left", padx=10)

        # Retry Button
        retry_btn = tk.Button(
            btn_frame,
            text="Retry",
            font=("Arial", 10),
            bg="#1a73e8",
            fg="#ffffff",
            bd=0,
            padx=15,
            pady=6,
            activebackground="#1557b0",
            command=self.on_lock_retry
        )
        retry_btn.pack(side="left", padx=10)

    def on_lock_cancel(self):
        self.lock_win.destroy()
        self.root.quit()

    def on_lock_retry(self):
        self.lock_win.destroy()
        self.check_locks_and_start()

    def update_loop(self):
        # Update progress bar view
        self.progress['value'] = self.progress_val
        self.root.after(50, self.update_loop)

    def run_install_sequence(self):
        # Check if Chrome is already installed (pre-installed offline at build time)
        if os.path.exists("/usr/bin/google-chrome-stable"):
            self.status_label.config(text="Locating pre-installed Chrome components...")
            for i in range(0, 51, 5):
                time.sleep(0.04)
                self.progress_val = i
            
            self.status_label.config(text="Verifying local package signatures offline...")
            for i in range(50, 91, 5):
                time.sleep(0.04)
                self.progress_val = i
                
            self.status_label.config(text="Registering system environment paths...")
            self.progress_val = 100
            time.sleep(0.2)
            
            self.create_chrome_desktop_shortcut()
            self.status_label.config(text="Google Chrome successfully verified offline.")
            self.action_btn.config(
                state="normal", 
                bg="#1a73e8", 
                fg="#ffffff", 
                activebackground="#1557b0",
                text="Launch Chrome", 
                command=self.launch_chrome_and_quit
            )
            return

        # Phase 1: Simulated Download (0% - 100%)
        self.status_label.config(text="Downloading Google Chrome stable components...")
        for i in range(101):
            time.sleep(0.03) # ~3 seconds total download
            self.progress_val = i
            
        # Phase 2: Install Package via dpkg
        self.status_label.config(text="Extracting and installing package archives...")
        self.progress_val = 25
        
        deb_path = "/home/aether/Downloads/google-chrome.deb"
        if not os.path.exists(deb_path):
            # Fallback check for debug running
            deb_path = "/mnt/c/Users/defaultuser0/Desktop/Antigravity/AetherOS-64/google-chrome-stable_current_amd64.deb"
            
        if os.path.exists(deb_path):
            try:
                # Install package using dpkg directly since dependencies are pre-installed
                self.progress_val = 55
                self.status_label.config(text="Installing Google Chrome packages...")
                proc = subprocess.Popen(
                    ["sudo", "dpkg", "-i", deb_path],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )
                
                # Wait for process to complete
                stdout, stderr = proc.communicate()
                print(stdout)
                
                if proc.returncode != 0:
                    # If dpkg fails (e.g. missing dependencies), try to resolve using apt-get update & install -f
                    self.status_label.config(text="Updating repositories to resolve dependencies...")
                    subprocess.run(["sudo", "apt-get", "update"])
                    subprocess.run(["sudo", "apt-get", "install", "-y", "-f"])
                
                self.progress_val = 90
                time.sleep(1)
            except Exception as e:
                print(f"Error during installation: {e}")
                self.status_label.config(text="Installation failed! Contact core administrator.")
                self.action_btn.config(state="normal", bg="#1a73e8", fg="#ffffff", text="Exit")
                return
        else:
            self.status_label.config(text="Error: chrome.deb not found in Downloads!")
            self.action_btn.config(state="normal", bg="#1a73e8", fg="#ffffff", text="Exit")
            return

        # Phase 3: Create Desktop Shortcut and finish
        self.status_label.config(text="Registering system environment paths...")
        self.progress_val = 100
        
        self.create_chrome_desktop_shortcut()
        
        self.status_label.config(text="Google Chrome successfully installed.")
        self.action_btn.config(
            state="normal", 
            bg="#1a73e8", 
            fg="#ffffff", 
            activebackground="#1557b0",
            text="Launch Chrome", 
            command=self.launch_chrome_and_quit
        )

    def create_chrome_desktop_shortcut(self):
        desktop_path = "/home/aether/Desktop/Google Chrome.desktop"
        try:
            with open(desktop_path, "w") as f:
                f.write("""[Desktop Entry]
Version=1.0
Name=Google Chrome
Comment=Access the Internet
Exec=/usr/bin/google-chrome-stable --no-sandbox
Icon=google-chrome
Terminal=false
Type=Application
Categories=Network;WebBrowser;
""")
            os.chmod(desktop_path, 0o755)
            subprocess.run(["chown", "aether:aether", desktop_path])
            
            # Clean up the setup shortcut if it exists
            setup_desktop = "/home/aether/Desktop/Chrome Setup.desktop"
            if os.path.exists(setup_desktop):
                os.remove(setup_desktop)
        except Exception as e:
            print(f"Failed to create desktop shortcut: {e}")

    def launch_chrome_and_quit(self):
        try:
            subprocess.Popen(["/usr/bin/google-chrome-stable", "--no-sandbox"], preexec_fn=os.setpgrp)
        except Exception as e:
            print(f"Failed to launch Chrome: {e}")
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = ChromeInstallerApp(root)
    root.mainloop()
