#!/usr/bin/env python3
# ==============================================================
# AETHEROS LINUX — APPS DEPLOYER GUI WIZARD (JARVIS STYLE)
# ==============================================================

import os
import sys
import subprocess
import threading
import time
import tkinter as tk
import customtkinter as ctk

# Set appearance mode
ctk.set_appearance_mode("dark")

class AetherDeployerApp(ctk.CTk):
    def __init__(self, app_name):
        super().__init__()
        
        self.app_name = app_name
        self.display_name = app_name.upper()
        
        # Configure Window
        self.title("AETHERIS DEPLOYMENT PROTOCOL")
        width = 580
        height = 420
        
        # Center on screen
        screen_width = self.winfo_screenwidth()
        screen_height = self.winfo_screenheight()
        x = (screen_width - width) // 2
        y = (screen_height - height) // 2
        self.geometry(f"{width}x{height}+{x}+{y}")
        
        # Frameless window with Aether theme
        self.overrideredirect(True)
        self.configure(fg_color="#07080e")
        self.attributes("-topmost", True)
        
        # Frame borders (cyan glow effect)
        self.outer_frame = ctk.CTkFrame(self, fg_color="#07080e", border_color="#06b6d4", border_width=2, corner_radius=12)
        self.outer_frame.pack(fill="both", expand=True, padx=4, pady=4)
        
        # Layout weights
        self.outer_frame.grid_rowconfigure(0, weight=0) # Title
        self.outer_frame.grid_rowconfigure(1, weight=0) # Deploy Header
        self.outer_frame.grid_rowconfigure(2, weight=0) # Progress / Info
        self.outer_frame.grid_rowconfigure(3, weight=1) # Log terminal
        self.outer_frame.grid_rowconfigure(4, weight=0) # Button pane
        self.outer_frame.grid_columnconfigure(0, weight=1)
        
        # 1. Title Bar
        self.title_bar = ctk.CTkFrame(self.outer_frame, height=40, fg_color="#0d0e15", corner_radius=0)
        self.title_bar.grid(row=0, column=0, sticky="ew", padx=2, pady=(2, 0))
        self.title_bar.grid_columnconfigure(0, weight=1)
        
        self.title_label = ctk.CTkLabel(
            self.title_bar, text="AETHERIS DEPLOYER v1.0", 
            font=ctk.CTkFont(family="monospace", size=13, weight="bold"),
            text_color="#06b6d4"
        )
        self.title_label.grid(row=0, column=0, padx=15, sticky="w")
        
        # Minimize window-drag behavior
        self.title_bar.bind("<Button-1>", self.start_drag)
        self.title_bar.bind("<B1-Motion>", self.drag)
        self.title_label.bind("<Button-1>", self.start_drag)
        self.title_label.bind("<B1-Motion>", self.drag)
        
        # 2. Deploy Header
        self.header_label = ctk.CTkLabel(
            self.outer_frame, 
            text=f"INITIALIZING SYSTEM ARCHIVE: {self.display_name}", 
            font=ctk.CTkFont(family="monospace", size=16, weight="bold"),
            text_color="#00ffff"
        )
        self.header_label.grid(row=1, column=0, pady=(20, 10))
        
        # 3. Dynamic Progress Information
        self.status_label = ctk.CTkLabel(
            self.outer_frame, 
            text="Syncing package libraries...", 
            font=ctk.CTkFont(family="monospace", size=11),
            text_color="#94a3b8"
        )
        self.status_label.grid(row=2, column=0, pady=(0, 10))
        
        self.progress_bar = ctk.CTkProgressBar(
            self.outer_frame, width=480, height=8,
            fg_color="#1e293b", progress_color="#00ffff",
            corner_radius=4
        )
        self.progress_bar.grid(row=2, column=0, pady=(25, 15))
        self.progress_bar.set(0.0)
        
        # 4. Terminal Log Window
        self.log_textbox = ctk.CTkTextbox(
            self.outer_frame, 
            fg_color="#020408", 
            text_color="#38bdf8",
            font=ctk.CTkFont(family="monospace", size=10),
            corner_radius=6,
            border_color="#1e293b",
            border_width=1
        )
        self.log_textbox.grid(row=3, column=0, padx=25, pady=(5, 15), sticky="nsew")
        self.log_textbox.configure(state="disabled")
        
        # 5. Buttons Pane
        self.btn_frame = ctk.CTkFrame(self.outer_frame, fg_color="transparent")
        self.btn_frame.grid(row=4, column=0, pady=(0, 20), sticky="ew")
        self.btn_frame.grid_columnconfigure(0, weight=1)
        
        self.action_btn = ctk.CTkButton(
            self.btn_frame, 
            text="DEPLOYING...", 
            font=ctk.CTkFont(family="monospace", size=12, weight="bold"),
            width=150, height=35,
            fg_color="#1e293b", text_color="#64748b",
            hover=False,
            command=self.close_deployer
        )
        self.action_btn.grid(row=0, column=0)
        
        # Start installation loop
        self.is_done = False
        self.return_code = None
        self.after(500, self.start_installation)
        self.after(100, self.animate_status)
        
    def start_drag(self, event):
        self.x = event.x
        self.y = event.y

    def drag(self, event):
        deltax = event.x - self.x
        deltay = event.y - self.y
        x = self.winfo_x() + deltax
        y = self.winfo_y() + deltay
        self.geometry(f"+{x}+{y}")
        
    def write_log(self, text):
        self.log_textbox.configure(state="normal")
        self.log_textbox.insert(tk.END, text + "\n")
        self.log_textbox.see(tk.END)
        self.log_textbox.configure(state="disabled")
        
    def start_installation(self):
        threading.Thread(target=self.run_install_process, daemon=True).start()
        
    def animate_status(self):
        if self.is_done:
            return
            
        # Animate progress bar slightly to show activity
        current = self.progress_bar.get()
        if current < 0.9:
            self.progress_bar.set(current + 0.005)
            
        # Rotate status texts if needed
        self.after(100, self.animate_status)
        
    def run_install_process(self):
        self.write_log(f"==> Initiating Aetheris Deployment Framework for app: {self.app_name}")
        self.write_log("==> Spawning backend chroot installer session...")
        
        cmd = ["sudo", "/usr/local/bin/aether-install", "--no-gui", self.app_name]
        
        try:
            # We run the actual installer script with --no-gui
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            while True:
                line = proc.stdout.readline()
                if not line:
                    break
                clean_line = line.strip()
                if clean_line:
                    self.write_log(clean_line)
                    # Dynamically update status label based on what is happening
                    if "Downloading" in clean_line or "wget" in clean_line:
                        self.status_label.configure(text="Downloading remote archive packages...")
                    elif "Extracting" in clean_line or "tar" in clean_line:
                        self.status_label.configure(text="Decompressing system files...")
                    elif "Installing" in clean_line or "dpkg" in clean_line or "apt-get" in clean_line:
                        self.status_label.configure(text="Configuring OS library entries...")
                    elif "complete" in clean_line.lower() or "successful" in clean_line.lower():
                        self.status_label.configure(text="System link established successfully.")
                        
            proc.wait()
            self.return_code = proc.returncode
            
        except Exception as e:
            self.write_log(f"Error during execution: {str(e)}")
            self.return_code = 1
            
        self.is_done = True
        self.after(0, self.handle_completion)
        
    def handle_completion(self):
        if self.return_code == 0:
            self.progress_bar.set(1.0)
            self.status_label.configure(text="DEPLOYMENT COMPLETE.", text_color="#10b981")
            self.header_label.configure(text=f"SUCCESS: {self.display_name} ACTIVE", text_color="#10b981")
            self.write_log("\n[SUCCESS] Deployment protocol executed successfully.")
            self.action_btn.configure(
                text="CLOSE", 
                fg_color="#10b981", 
                hover_color="#059669", 
                text_color="#ffffff",
                hover=True
            )
        else:
            self.status_label.configure(text="DEPLOYMENT FAULT / TERMINATED.", text_color="#ef4444")
            self.header_label.configure(text=f"ERROR: {self.display_name} FAULT", text_color="#ef4444")
            self.write_log(f"\n[FAULT] Deployment returned error code {self.return_code}")
            self.action_btn.configure(
                text="CLOSE", 
                fg_color="#ef4444", 
                hover_color="#dc2626", 
                text_color="#ffffff",
                hover=True
            )
            
    def close_deployer(self):
        if self.is_done:
            self.destroy()
            sys.exit(self.return_code)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: aether-installer-gui.py <app_name>")
        sys.exit(1)
        
    app_name = sys.argv[1]
    deployer = AetherDeployerApp(app_name)
    deployer.mainloop()
