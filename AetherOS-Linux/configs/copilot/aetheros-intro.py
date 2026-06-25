#!/usr/bin/env python3
import os
import sys
import tkinter as tk
import customtkinter as ctk
import math
import time

ctk.set_appearance_mode("dark")

class AetherDesktopIntro(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        # Make full screen and borderless
        self.overrideredirect(True)
        
        # Bind Configure to handle dynamic window scaling/resolution changes
        self.bind("<Configure>", self.on_resize)
        
        # Enable fullscreen attributes for maximum layout fit
        self.attributes("-fullscreen", True)
        self.attributes("-topmost", True)
        self.configure(fg_color="#05060b")
        
        # Dimensions
        self.width = self.winfo_screenwidth()
        self.height = self.winfo_screenheight()
        self.cx = self.width // 2
        self.cy = self.height // 2
        
        # Canvas for high-tech holographic vector graphics
        self.canvas = tk.Canvas(self, bg="#05060b", highlightthickness=0, width=self.width, height=self.height)
        self.canvas.pack(fill="both", expand=True)
        
        # Animation variables
        self.angle1 = 0.0
        self.angle2 = 0.0
        self.pulse = 0.0
        self.pulse_dir = 1
        
        # Diagnostic messages
        self.messages = [
            "SYS: INITIALIZING DIRECT DEPLOYMENT LINK...",
            "SYS: SHIFTING GRAPHICS BUFFER TO 1920x1080...",
            "SYS: LOADING COPROCESSOR NEURAL NETWORK MODULES...",
            "SYS: STAGING SYSTEM STORAGE /dev/sda...",
            "SYS: AETHER OS CORE READY. ACCESS GRANTED."
        ]
        
        # Start animations
        self.start_time = time.time()
        self.duration = 4.5 # 4.5 seconds animation, then 0.5 seconds fade
        
        self.initialized = False
        self.draw_hud_decorations()
        self.animate()
        
    def on_resize(self, event):
        # Ignore initial zero sizes
        if event.width < 100 or event.height < 100:
            return
            
        if not self.initialized or event.width != self.width or event.height != self.height:
            self.width = event.width
            self.height = event.height
            self.cx = self.width // 2
            self.cy = self.height // 2
            self.initialized = True
            
            # Redraw everything to fit the new resolution
            self.canvas.delete("all")
            self.draw_hud_decorations()
            
    def draw_hud_decorations(self):
        w = self.width
        h = self.height
        
        # Corner brackets (cyan glow)
        # Top-left
        self.canvas.create_line(40, 40, 100, 40, fill="#06b6d4", width=2)
        self.canvas.create_line(40, 40, 40, 100, fill="#06b6d4", width=2)
        # Top-right
        self.canvas.create_line(w - 40, 40, w - 100, 40, fill="#06b6d4", width=2)
        self.canvas.create_line(w - 40, 40, w - 40, 100, fill="#06b6d4", width=2)
        # Bottom-left
        self.canvas.create_line(40, h - 40, 100, h - 40, fill="#06b6d4", width=2)
        self.canvas.create_line(40, h - 40, 40, h - 100, fill="#06b6d4", width=2)
        # Bottom-right
        self.canvas.create_line(w - 40, h - 40, w - 100, h - 40, fill="#06b6d4", width=2)
        self.canvas.create_line(w - 40, h - 40, w - 40, h - 100, fill="#06b6d4", width=2)
        
        # Background Grid Lines (subtle dark cyan)
        grid_size = 80
        for x in range(0, w, grid_size):
            self.canvas.create_line(x, 0, x, h, fill="#083344", width=1, dash=(2, 10))
        for y in range(0, h, grid_size):
            self.canvas.create_line(0, y, w, y, fill="#083344", width=1, dash=(2, 10))
            
        # HUD Brackets surrounding center
        self.canvas.create_rectangle(self.cx - 220, self.cy - 220, self.cx + 220, self.cy + 220, outline="#0891b2", width=1, dash=(4, 4))
        
        # Titles
        self.canvas.create_text(self.cx, self.cy - 280, text="AETHER OS v0.1", fill="#00ffff", font=("Courier", 24, "bold"), justify="center")
        self.canvas.create_text(self.cx, self.cy - 250, text="NEURAL INTERFACE PROTOCOL", fill="#06b6d4", font=("Courier", 12, "normal"), justify="center")
        
        # Progress Bar Frame
        self.canvas.create_rectangle(self.cx - 250, self.cy + 260, self.cx + 250, self.cy + 275, outline="#0891b2", width=1)
        self.progress_bar = self.canvas.create_rectangle(self.cx - 248, self.cy + 262, self.cx - 248, self.cy + 273, fill="#06b6d4", width=0)
        
        # Log Box
        self.log_text = self.canvas.create_text(self.cx - 240, self.cy + 300, text="", fill="#38bdf8", font=("Courier", 10, "normal"), anchor="nw")

    def animate(self):
        elapsed = time.time() - self.start_time
        
        if elapsed >= self.duration:
            # Initiate fade out
            self.fade_out()
            return
            
        # Update angles
        self.angle1 += 0.05
        self.angle2 -= 0.03
        
        # Pulse core
        self.pulse += 0.05 * self.pulse_dir
        if self.pulse > 1.0:
            self.pulse = 1.0
            self.pulse_dir = -1
        elif self.pulse < 0.0:
            self.pulse = 0.0
            self.pulse_dir = 1
            
        # Clear dynamic elements
        self.canvas.delete("dynamic")
        
        # Draw tech rings
        # Ring 1 (outer gear)
        r1 = 180
        for i in range(12):
            a = self.angle1 + i * (2 * math.pi / 12)
            x1 = self.cx + r1 * math.cos(a)
            y1 = self.cy + r1 * math.sin(a)
            x2 = self.cx + (r1 + 15) * math.cos(a)
            y2 = self.cy + (r1 + 15) * math.sin(a)
            self.canvas.create_line(x1, y1, x2, y2, fill="#06b6d4", width=2, tags="dynamic")
            
        self.canvas.create_oval(self.cx - r1, self.cy - r1, self.cx + r1, self.cy + r1, outline="#0891b2", width=2, tags="dynamic")
        
        # Ring 2 (inner dashed)
        r2 = 140
        self.canvas.create_arc(self.cx - r2, self.cy - r2, self.cx + r2, self.cy + r2, start=math.degrees(self.angle2), extent=120, outline="#00ffff", width=3, style="arc", tags="dynamic")
        self.canvas.create_arc(self.cx - r2, self.cy - r2, self.cx + r2, self.cy + r2, start=math.degrees(self.angle2) + 180, extent=120, outline="#00ffff", width=3, style="arc", tags="dynamic")
        
        # Ring 3 (central spokes)
        r3 = 90
        self.canvas.create_oval(self.cx - r3, self.cy - r3, self.cx + r3, self.cy + r3, outline="#0891b2", width=1, dash=(1, 5), tags="dynamic")
        
        # Glowing Core
        core_r = 40 + int(10 * self.pulse)
        self.canvas.create_oval(self.cx - core_r, self.cy - core_r, self.cx + core_r, self.cy + core_r, fill="#083344", outline="#06b6d4", width=2, tags="dynamic")
        self.canvas.create_oval(self.cx - 15, self.cy - 15, self.cx + 15, self.cy + 15, fill="#00ffff", outline="", tags="dynamic")
        
        # Update progress bar
        pct = elapsed / self.duration
        if pct > 1.0: pct = 1.0
        bar_w = int(496 * pct)
        self.canvas.coords(self.progress_bar, self.cx - 248, self.cy + 262, self.cx - 248 + bar_w, self.cy + 273)
        
        # Update diagnostic logs
        msg_idx = min(int(elapsed / (self.duration / len(self.messages))), len(self.messages) - 1)
        visible_logs = "\n".join(self.messages[:msg_idx + 1])
        self.canvas.itemconfig(self.log_text, text=visible_logs)
        
        # Re-schedule frame
        self.after(16, self.animate)
        
    def fade_out(self):
        self.fade_start = time.time()
        self.animate_fade()
        
    def animate_fade(self):
        fade_elapsed = time.time() - self.fade_start
        alpha = 1.0 - (fade_elapsed / 0.5)
        if alpha <= 0.0:
            self.attributes("-alpha", 0.0)
            self.destroy()
            sys.exit(0)
        else:
            self.attributes("-alpha", alpha)
            self.after(16, self.animate_fade)

if __name__ == "__main__":
    # Wait for graphical session to settle down
    time.sleep(0.5)
    app = AetherDesktopIntro()
    app.mainloop()
