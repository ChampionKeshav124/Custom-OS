import os
import sys
import json
import subprocess
import threading
import traceback
import urllib.request
import urllib.error
import math
import tkinter as tk
import customtkinter as ctk

# Set app appearance
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

API_KEY_PATH = os.path.expanduser("~/.config/gemini_key.txt")

class CopilotApp(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        # Screen dimensions configuration (docks to the right of current resolution)
        screen_width, screen_height = self.get_screen_dimensions()
        
        width = 320
        height = screen_height
        x_pos = screen_width - width
        y_pos = 0
        
        self.geometry(f"{width}x{height}+{x_pos}+{y_pos}")
        self.title("AETHER PILOT")
        
        # We rely on Openbox rc.xml application rules under Linux to make the window borderless/topmost/focused.
        # For non-Linux platforms, we keep overrideredirect as fallback.
        if not sys.platform.startswith("linux"):
            self.overrideredirect(True)
            
        self.attributes("-topmost", True)
        self.focus_force()
        
        self.api_key = self.load_api_key()
        if self.api_key:
            os.environ["GEMINI_API_KEY"] = self.api_key
        self.history = [
            {
                "role": "user",
                "parts": [{"text": (
                    "You are Aether Pilot, a local AI system assistant running directly inside the custom Linux OS (AetherOS) virtual machine.\n"
                    "You have administrative rights. If the user asks you to read, write, modify, list, delete files, run system checks, or manage settings, you MUST execute actual code to get the facts.\n"
                    "To execute actions, write a python code block starting with '```python'. Raw bash blocks starting with '```bash' are BLOCKED on this system and will fail. If you need to run any system command, terminal command, or installer, you MUST invoke it inside a python block using python's `subprocess` module.\n"
                    "If the user asks you to install any application or software (such as Chrome, Firefox, Edge, OpenOffice, VLC, etc.), you MUST execute a python block that calls `sudo aether-install <app_name>` via subprocess. For example:\n"
                    "```python\n"
                    "import subprocess\n"
                    "res = subprocess.run(['sudo', 'aether-install', 'chrome'], capture_output=True, text=True)\n"
                    "print(res.stdout)\n"
                    "print(res.stderr)\n"
                    "```\n"
                    "Use app names like 'chrome', 'firefox', 'edge', 'openoffice', or any other valid package name for standard system package manager installation. The script will automatically trigger a custom Jarvis loading page on the desktop and complete the installation.\n"
                    "If the user wants you to draw, paint, create, or generate an image, write a python block that imports `aether_image` from `/home/aether/copilot` and runs `aether_image.generate(\"prompt string\")` to generate and save it onto their Desktop.\n"
                    "The system will execute this block automatically and feed the stdout/stderr output back into your context as a System Output turn.\n"
                    "Wait for the execution output before explaining results. Be concise, direct, and refer to the user as 'Sir'."
                )}]
            },
            {
                "role": "model",
                "parts": [{"text": "Understood, Sir. I am Aether Pilot. I have full system access and am ready to execute commands, manage files, and generate images for you."}]
            }
        ]
        
        # Main Frame Layout
        self.grid_rowconfigure(0, weight=0)  # Title bar
        self.grid_rowconfigure(1, weight=1)  # Content area
        self.grid_columnconfigure(0, weight=1)
        
        # 1. Custom Title Bar Frame
        self.title_bar = ctk.CTkFrame(self, height=45, corner_radius=0, fg_color="#0d0e15")
        self.title_bar.grid(row=0, column=0, sticky="ew")
        self.title_bar.grid_rowconfigure(0, weight=1)
        self.title_bar.grid_columnconfigure(0, weight=1)
        
        self.title_label = ctk.CTkLabel(
            self.title_bar, text="AETHER PILOT", 
            font=ctk.CTkFont(family="Arial", size=14, weight="bold"),
            text_color="#3b82f6"
        )
        self.title_label.grid(row=0, column=0, padx=15, sticky="w")
        
        self.close_btn = ctk.CTkButton(
            self.title_bar, text="X", width=30, height=25,
            fg_color="transparent", hover_color="#ef4444", text_color="#ffffff",
            command=self.destroy
        )
        self.close_btn.grid(row=0, column=1, padx=10, sticky="e")
        
        self.reset_btn = ctk.CTkButton(
            self.title_bar, text="⚙", width=30, height=25,
            fg_color="transparent", hover_color="#3b82f6", text_color="#ffffff",
            command=self.reset_api_key
        )
        self.reset_btn.grid(row=0, column=1, padx=(0, 45), sticky="e")

        # Drag-to-move window bindings for frameless window support
        self.title_bar.bind("<Button-1>", self.start_drag)
        self.title_bar.bind("<B1-Motion>", self.drag)
        self.title_label.bind("<Button-1>", self.start_drag)
        self.title_label.bind("<B1-Motion>", self.drag)
        
        # 2. Main Content Container
        self.content_frame = ctk.CTkFrame(self, corner_radius=0, fg_color="#07080e")
        self.content_frame.grid(row=1, column=0, sticky="nsew")
        self.content_frame.grid_rowconfigure(0, weight=1)
        self.content_frame.grid_columnconfigure(0, weight=1)
        
        self.show_boot_intro()
        self.check_resolution()

    def get_screen_dimensions(self):
        # Default fallback
        width = self.winfo_screenwidth()
        height = self.winfo_screenheight()
        
        if sys.platform.startswith("linux"):
            try:
                proc = subprocess.run(
                    ["xdotool", "getdisplaygeometry"],
                    capture_output=True, text=True, timeout=1.0,
                    env=os.environ
                )
                if proc.returncode == 0 and proc.stdout:
                    parts = proc.stdout.strip().split()
                    if len(parts) >= 2:
                        width = int(parts[0])
                        height = int(parts[1])
            except Exception:
                pass
        return width, height

    def check_resolution(self):
        try:
            screen_width, screen_height = self.get_screen_dimensions()
            
            # If the screen size has changed, reposition the sidebar
            if not hasattr(self, "_last_screen_width") or self._last_screen_width != screen_width or self._last_screen_height != screen_height:
                self._last_screen_width = screen_width
                self._last_screen_height = screen_height
                
                width = 320
                height = screen_height
                x_pos = screen_width - width
                y_pos = 0
                
                self.geometry(f"{width}x{height}+{x_pos}+{y_pos}")
        except:
            pass
        self.after(500, self.check_resolution)

    def show_boot_intro(self):
        for widget in self.content_frame.winfo_children():
            widget.destroy()
            
        self.boot_frame = ctk.CTkFrame(self.content_frame, fg_color="transparent")
        self.boot_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        # High-tech Tkinter Canvas for spinning hologram / arc reactor
        self.boot_canvas = tk.Canvas(
            self.boot_frame, width=220, height=220, 
            bg="#07080e", highlightthickness=0
        )
        self.boot_canvas.pack(pady=(20, 10))
        
        # Tech Title Label inside boot frame
        self.boot_title = ctk.CTkLabel(
            self.boot_frame, text="A.E.T.H.E.R  O.S",
            font=ctk.CTkFont(family="Arial", size=14, weight="bold"),
            text_color="#00f0ff"
        )
        self.boot_title.pack(pady=2)
        
        self.boot_subtitle = ctk.CTkLabel(
            self.boot_frame, text="SYSTEM BOT INITIALIZATION",
            font=ctk.CTkFont(family="Arial", size=9, weight="bold"),
            text_color="#64748b"
        )
        self.boot_subtitle.pack(pady=(0, 10))
        
        # High-tech scrolling boot log label
        self.boot_log = ctk.CTkLabel(
            self.boot_frame, text="Initializing Aether kernel...",
            font=ctk.CTkFont(family="Courier", size=10),
            text_color="#10b981", # Emerald green console
            anchor="w", justify="left"
        )
        self.boot_log.pack(fill="x", padx=10, pady=5)
        
        # Progress Bar
        self.boot_progress = ctk.CTkProgressBar(
            self.boot_frame, width=280, height=8,
            fg_color="#1e293b", progress_color="#00f0ff"
        )
        self.boot_progress.pack(pady=10)
        self.boot_progress.set(0.0)
        
        # Tech specs
        self.boot_specs = ctk.CTkLabel(
            self.boot_frame, text="CORE_LINK: ONLINE  |  SYS_PORT: 5122",
            font=ctk.CTkFont(family="Arial", size=9),
            text_color="#475569"
        )
        self.boot_specs.pack(pady=5)
        
        self.boot_val = 0.0
        self.boot_angle = 0.0
        
        # Define high tech log messages
        self.boot_messages = [
            "SYSTEM: LOADING CORE CONFIGURATION...",
            "SYSTEM: MOUNTING VIRTUAL FILESYSTEMS...",
            "SYSTEM: SCANNING STORAGE FOR KEYS...",
            "SYSTEM: DISCOVERING keys.iso CD-ROM...",
            "SYSTEM: READING MOUNT POINT /media/aether/KEYS...",
            "SYSTEM: PARSING VM PROPERTY FALLBACKS...",
            "SYSTEM: DECRYPTING API TOKEN PROTOCOLS...",
            "SYSTEM: INJECTING GEMINI_API_KEY ENVS...",
            "SYSTEM: ESTABLISHING COPROCESSOR LINK...",
            "SYSTEM: VERIFYING INTERACTIVE INTERFACE...",
            "SYSTEM: SUCCESS! CONNECTING PILOT MODULE...",
            "SYSTEM: ONLINE. WELCOME BACK, SIR."
        ]
        
        # Start animation loop
        self.animate_boot_intro()

    def animate_boot_intro(self):
        if not hasattr(self, "boot_canvas") or not self.boot_canvas.winfo_exists():
            return
            
        self.boot_val += 0.02
        if self.boot_val > 1.0:
            self.boot_val = 1.0
            
        self.boot_progress.set(self.boot_val)
        self.boot_angle += 0.08  # Radians rotation speed
        
        # Clear canvas
        self.boot_canvas.delete("all")
        
        # Center of canvas
        cx, cy = 110, 110
        
        # 1. Pulsing core glow circle
        pulse_r = 18 + 4 * math.sin(self.boot_angle * 3)
        self.boot_canvas.create_oval(
            cx - pulse_r, cy - pulse_r, cx + pulse_r, cy + pulse_r,
            fill="#0f172a", outline="#00f0ff", width=2
        )
        # Inner core fill color based on pulse
        core_color = "#00f0ff" if int(self.boot_angle * 10) % 2 == 0 else "#0ea5e9"
        self.boot_canvas.create_oval(
            cx - 8, cy - 8, cx + 8, cy + 8,
            fill=core_color, outline=""
        )
        
        # 2. Concentric circle gridlines
        self.boot_canvas.create_oval(cx - 95, cy - 95, cx + 95, cy + 95, outline="#0f172a", width=1)
        self.boot_canvas.create_oval(cx - 85, cy - 85, cx + 85, cy + 85, outline="#1e293b", width=1)
        self.boot_canvas.create_oval(cx - 55, cy - 55, cx + 55, cy + 55, outline="#1e293b", width=1)
        
        # 3. Outer rotating dotted / dashed ring
        # Draw 16 arcs to make a dashed ring
        num_arcs = 16
        arc_angle_step = 360 / num_arcs
        for i in range(num_arcs):
            start_ang = (self.boot_angle * 57.2958) + i * arc_angle_step
            # Draw arc
            self.boot_canvas.create_arc(
                cx - 85, cy - 85, cx + 85, cy + 85,
                start=start_ang, extent=arc_angle_step / 2,
                style="arc", outline="#00f0ff", width=2
            )
            
        # 4. Inner counter-rotating ring with small extent
        num_inner_arcs = 8
        inner_step = 360 / num_inner_arcs
        for i in range(num_inner_arcs):
            # Counter rotation: minus sign
            start_ang = (-self.boot_angle * 57.2958 * 1.5) + i * inner_step
            self.boot_canvas.create_arc(
                cx - 55, cy - 55, cx + 55, cy + 55,
                start=start_ang, extent=inner_step / 3,
                style="arc", outline="#0284c7", width=3
            )
            
        # 5. Rotating spoke lines (Arc Reactor pattern)
        num_spokes = 8
        for i in range(num_spokes):
            theta = self.boot_angle + i * (2 * math.pi / num_spokes)
            x1 = cx + 30 * math.cos(theta)
            y1 = cy + 30 * math.sin(theta)
            x2 = cx + 50 * math.cos(theta)
            y2 = cy + 50 * math.sin(theta)
            self.boot_canvas.create_line(x1, y1, x2, y2, fill="#00f0ff", width=2)
            
        # 6. Tech crosshairs / brackets
        bracket_r = 102
        # Top-Left bracket
        self.boot_canvas.create_arc(
            cx - bracket_r, cy - bracket_r, cx + bracket_r, cy + bracket_r,
            start=120, extent=30, style="arc", outline="#64748b", width=1
        )
        # Top-Right bracket
        self.boot_canvas.create_arc(
            cx - bracket_r, cy - bracket_r, cx + bracket_r, cy + bracket_r,
            start=30, extent=30, style="arc", outline="#64748b", width=1
        )
        # Bottom bracket
        self.boot_canvas.create_arc(
            cx - bracket_r, cy - bracket_r, cx + bracket_r, cy + bracket_r,
            start=255, extent=30, style="arc", outline="#64748b", width=1
        )
        
        # Update log message based on progress
        msg_idx = min(int(self.boot_val * len(self.boot_messages)), len(self.boot_messages) - 1)
        self.boot_log.configure(text=self.boot_messages[msg_idx])
        
        if self.boot_val < 1.0:
            self.after(50, self.animate_boot_intro)
        else:
            # Settle on final screen for 200ms
            self.after(200, self.finish_boot_intro)

    def finish_boot_intro(self):
        # Perform final key loading
        self.api_key = self.load_api_key()
        if self.api_key:
            os.environ["GEMINI_API_KEY"] = self.api_key
            
        # Destroy all boot intro widgets
        if hasattr(self, "boot_frame") and self.boot_frame.winfo_exists():
            self.boot_frame.destroy()
            
        # Transition
        if not self.api_key:
            self.show_config_screen()
        else:
            self.show_chat_screen()

    def start_drag(self, event):
        self.x = event.x
        self.y = event.y

    def drag(self, event):
        deltax = event.x - self.x
        deltay = event.y - self.y
        x = self.winfo_x() + deltax
        y = self.winfo_y() + deltay
        self.geometry(f"+{x}+{y}")
            
    def add_paste_support(self, widget):
        if hasattr(widget, "_entry"):
            inner_widget = widget._entry
        elif hasattr(widget, "_textbox"):
            inner_widget = widget._textbox
        else:
            inner_widget = widget

        def paste_action(event=None):
            try:
                clipboard_text = None
                
                # 1. Try xclip (extremely reliable X11 selection client)
                try:
                    proc = subprocess.run(
                        ["xclip", "-selection", "clipboard", "-o"],
                        capture_output=True, text=True, timeout=1.5,
                        env=os.environ
                    )
                    if proc.returncode == 0 and proc.stdout:
                        clipboard_text = proc.stdout
                except Exception:
                    pass

                # 2. Try xsel (alternative X11 selection client)
                if not clipboard_text:
                    try:
                        proc = subprocess.run(
                            ["xsel", "--clipboard", "--output"],
                            capture_output=True, text=True, timeout=1.5,
                            env=os.environ
                        )
                        if proc.returncode == 0 and proc.stdout:
                            clipboard_text = proc.stdout
                    except Exception:
                        pass

                # 3. Fall back to standard Tkinter clipboard
                if not clipboard_text:
                    try:
                        clipboard_text = widget.clipboard_get()
                    except tk.TclError:
                        try:
                            clipboard_text = widget.clipboard_get(type="UTF8_STRING")
                        except tk.TclError:
                            pass

                if not clipboard_text or not clipboard_text.strip():
                    self.show_paste_error(widget, "Text not identified (Clipboard is empty)")
                    return "break"
                
                # Check for file URIs
                if clipboard_text.startswith("file://") and hasattr(self, "msg_entry"):
                    lines = clipboard_text.strip().split('\n')
                    files = []
                    for line in lines:
                        if line.startswith("file://"):
                            import urllib.parse
                            path = urllib.parse.unquote(line[7:])
                            if os.path.exists(path):
                                files.append(path)
                    
                    if files:
                        self.handle_pasted_files(files)
                        return "break"
                
                # Foolproof placeholder check: clear if text matches placeholder or if internal flag is active
                current_text = inner_widget.get()
                placeholder = getattr(widget, "_placeholder_text", "")
                if (placeholder and current_text == placeholder) or getattr(widget, "_placeholder_active", False) or current_text == "[Text not identified]":
                    inner_widget.delete(0, tk.END)
                    widget._placeholder_active = False
                    if hasattr(widget, "_show"):
                        inner_widget.configure(show=widget._show)
                    if hasattr(widget, "_text_color"):
                        t_color = widget._text_color
                        if isinstance(t_color, (list, tuple)) and len(t_color) >= 2:
                            t_color = t_color[1] if ctk.get_appearance_mode().lower() == "dark" else t_color[0]
                        inner_widget.configure(foreground=t_color)
                
                # Standard paste
                try:
                    inner_widget.delete("sel.first", "sel.last")
                except:
                    pass
                inner_widget.insert("insert", clipboard_text)
            except Exception as e:
                self.show_paste_error(widget, f"Text not identified: {str(e)}")
            return "break"

        inner_widget.bind("<Control-v>", paste_action, add="+")
        inner_widget.bind("<Control-V>", paste_action, add="+")
        inner_widget.bind("<Shift-Insert>", paste_action, add="+")
        
        # Right-click context menu for paste
        menu = tk.Menu(inner_widget, tearoff=0)
        menu.add_command(label="Paste", command=paste_action)
        
        def show_menu(event):
            menu.post(event.x_root, event.y_root)
            
        inner_widget.bind("<Button-3>", show_menu, add="+")

        # Clear placeholder/error on focus and restore password masking if needed
        def on_focus_in(event):
            current = inner_widget.get()
            placeholder = getattr(widget, "_placeholder_text", "")
            if current == "[Text not identified]" or (placeholder and current == placeholder) or getattr(widget, "_placeholder_active", False):
                inner_widget.delete(0, tk.END)
                widget._placeholder_active = False
                if hasattr(widget, "_show"):
                    inner_widget.configure(show=widget._show)
                if hasattr(widget, "_text_color"):
                    t_color = widget._text_color
                    if isinstance(t_color, (list, tuple)) and len(t_color) >= 2:
                        t_color = t_color[1] if ctk.get_appearance_mode().lower() == "dark" else t_color[0]
                    inner_widget.configure(foreground=t_color)

        inner_widget.bind("<FocusIn>", on_focus_in, add="+")

    def show_paste_error(self, widget, message):
        try:
            # Display error in placeholder so it doesn't become actual text value
            original_placeholder = getattr(widget, "_placeholder_text", "")
            if hasattr(widget, "configure"):
                widget.configure(placeholder_text=f"Error: {message}")
            
            # Reset after 3 seconds
            def reset():
                if hasattr(widget, "configure"):
                    widget.configure(placeholder_text=original_placeholder)
            self.after(3000, reset)
        except:
            pass

    def handle_pasted_files(self, files):
        for filepath in files:
            filename = os.path.basename(filepath)
            self.add_message_bubble("You", f"[Pasted File: {filename}] ({filepath})")
            
            is_image = filepath.lower().endswith(('.png', '.jpg', '.jpeg', '.gif', '.bmp'))
            if is_image:
                self.add_message_bubble("System", f"Loading image: {filename}...")
                self.add_message_bubble("You", f"[IMAGE: {filepath}]")
                
                try:
                    from PIL import Image
                    import io
                    import base64
                    
                    pil_img = Image.open(filepath)
                    if pil_img.mode == 'RGBA':
                        pil_img = pil_img.convert('RGB')
                    
                    buffer = io.BytesIO()
                    pil_img.save(buffer, format="JPEG", quality=80)
                    img_data = base64.b64encode(buffer.getvalue()).decode('utf-8')
                    
                    image_part = {
                        "inlineData": {
                            "mimeType": "image/jpeg",
                            "data": img_data
                        }
                    }
                    text_part = {
                        "text": f"Sir, I have pasted this image: {filename}. Please analyze it."
                    }
                    self.history.append({
                        "role": "user",
                        "parts": [image_part, text_part]
                    })
                    
                    self.msg_entry.configure(state="disabled")
                    self.send_btn.configure(state="disabled")
                    threading.Thread(target=self.query_pilot_loop, daemon=True).start()
                except Exception as e:
                    self.add_message_bubble("System", f"Failed to load image for Gemini: {str(e)}")
            else:
                try:
                    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                        content = f.read(3000)
                        if len(content) >= 3000:
                            content += "\n... [Truncated due to size] ..."
                    
                    self.add_message_bubble("System", f"Reading text file: {filename}...")
                    
                    self.history.append({
                        "role": "user",
                        "parts": [{"text": f"Sir, I have pasted the contents of file '{filename}' ({filepath}):\n\n```\n{content}\n```\n\nPlease summarize this file or tell me what actions you want to take on it."}]
                    })
                    
                    self.msg_entry.configure(state="disabled")
                    self.send_btn.configure(state="disabled")
                    threading.Thread(target=self.query_pilot_loop, daemon=True).start()
                except Exception as e:
                    self.add_message_bubble("System", f"Failed to read file: {str(e)}")

    def clean_key_string(self, raw_key):
        if not raw_key:
            return ""
        # Remove newlines, carriage returns, spaces, brackets, and quotes
        cleaned = raw_key.replace('\n', '').replace('\r', '').strip()
        cleaned = cleaned.strip("'\"").strip("[]").strip("'\"").strip()
        return cleaned

    def load_api_key(self):
        import glob, re
        
        # Automatically attempt to mount the secondary CD-ROM containing keys
        try:
            import subprocess
            # Try mounting by label KEYS first
            subprocess.run(
                "sudo mkdir -p /media/aether/KEYS && sudo mount -o ro /dev/disk/by-label/KEYS /media/aether/KEYS", 
                shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
            # Try mounting all sr devices (e.g. /dev/sr0, /dev/sr1, etc.) to detect dynamically
            for dev in glob.glob("/dev/sr*"):
                dev_name = os.path.basename(dev)
                subprocess.run(
                    f"sudo mkdir -p /media/aether/{dev_name} && sudo mount -o ro {dev} /media/aether/{dev_name}",
                    shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                )
        except:
            pass
            
        possible_paths = [
            "/media/aether/KEYS/api_key.txt",
            "/media/sf_AetherOS/api_key.txt",
            "/media/sf_AetherOS/gemini_key.txt",
            API_KEY_PATH,
            # 3. Home folder locations
            "/home/aether/api_key.txt",
            "/home/aether/gemini_key.txt",
            "/home/aether/Desktop/api_key.txt",
            "/home/aether/Desktop/gemini_key.txt",
            "/home/aether/Downloads/api_key.txt",
            # 4. Any mounted CD-ROM or USB drive
        ]

        # Also scan all removable media automatically
        possible_paths += glob.glob("/media/aether/*/api_key.txt")
        possible_paths += glob.glob("/media/*/api_key.txt")
        possible_paths += glob.glob("/mnt/*/api_key.txt")

        # Try every path
        for path in possible_paths:
            if os.path.exists(path):
                try:
                    with open(path, "r") as f:
                        key = self.clean_key_string(f.read())
                        if key and len(key) > 10 and "ENTER YOUR" not in key:
                            return key
                except:
                    continue

        # 5. Check environment variable
        key = self.clean_key_string(os.environ.get("GEMINI_API_KEY", ""))
        if key:
            return key

        # 6. Check VirtualBox Guest Property (injected by setup_vm.ps1 as fallback)
        try:
            import subprocess
            # Try running without sudo first
            proc = subprocess.run(
                ["VBoxControl", "guestproperty", "get", "/AetherOS/GeminiAPIKey"],
                capture_output=True, text=True, timeout=2.0
            )
            # If that fails, try absolute path /usr/bin/VBoxControl
            if proc.returncode != 0:
                proc = subprocess.run(
                    ["/usr/bin/VBoxControl", "guestproperty", "get", "/AetherOS/GeminiAPIKey"],
                    capture_output=True, text=True, timeout=2.0
                )
            # If that still fails, fall back to sudo
            if proc.returncode != 0:
                proc = subprocess.run(
                    ["sudo", "VBoxControl", "guestproperty", "get", "/AetherOS/GeminiAPIKey"],
                    capture_output=True, text=True, timeout=2.0
                )
            if proc.returncode == 0:
                output = proc.stdout.strip()
                if output.startswith("Value:"):
                    vbox_key = self.clean_key_string(output.split("Value:", 1)[1])
                    if vbox_key and "ENTER YOUR GEMINI API" not in vbox_key:
                        return vbox_key
        except:
            pass

        # 7. Check legacy config.py in home folder
        legacy_path = "/home/aether/config.py"
        if os.path.exists(legacy_path):
            try:
                with open(legacy_path, "r") as f:
                    content = f.read()
                match = re.search(r'GEMINI_API_KEY\s*=\s*[\'\"](.*?)[\'\"]', content)
                if match and match.group(1) and "ENTER" not in match.group(1):
                    return self.clean_key_string(match.group(1))
            except:
                pass

        return None

    def save_api_key(self, key):
        cleaned_key = self.clean_key_string(key)
        os.makedirs(os.path.dirname(API_KEY_PATH), exist_ok=True)
        with open(API_KEY_PATH, "w") as f:
            f.write(cleaned_key)
        self.api_key = cleaned_key
        os.environ["GEMINI_API_KEY"] = cleaned_key

    def show_config_screen(self):
        for widget in self.content_frame.winfo_children():
            widget.destroy()
            
        config_frame = ctk.CTkFrame(self.content_frame, fg_color="transparent")
        config_frame.pack(fill="both", expand=True, padx=20, pady=20)
        config_frame.grid_columnconfigure(0, weight=1)
        
        label = ctk.CTkLabel(
            config_frame, 
            text="Welcome to Aether Pilot\n\nPlease enter your Gemini API key\nto establish the system link:",
            font=ctk.CTkFont(size=13),
            justify="center"
        )
        label.grid(row=0, column=0, pady=(100, 20), sticky="ew")
        
        self.key_entry = ctk.CTkEntry(
            config_frame, placeholder_text="Enter API Key...", width=250
        )
        self.key_entry.grid(row=1, column=0, pady=10)
        self.key_entry.bind("<Return>", lambda e: self.handle_save_key())
        self.add_paste_support(self.key_entry)
        self.after(200, self.key_entry.focus)
        
        save_btn = ctk.CTkButton(
            config_frame, text="Connect Pilot", width=150,
            command=self.handle_save_key
        )
        save_btn.grid(row=2, column=0, pady=20)

        # Status indicator for keys.iso auto-detection
        self.status_label = ctk.CTkLabel(
            config_frame, 
            text="🔍 Scanning for keys.iso / VM properties...",
            font=ctk.CTkFont(family="Arial", size=11, slant="italic"),
            text_color="#64748b"
        )
        self.status_label.grid(row=3, column=0, pady=(10, 0))
        
        self.polling_for_key = True
        self.polling_counter = 0
        self.after(2000, self.poll_for_api_key)
        
    def handle_save_key(self):
        key = self.clean_key_string(self.key_entry.get())

        if not key or key == getattr(self.key_entry, "_placeholder_text", ""):
            self.show_paste_error(self.key_entry, "Invalid or empty API Key")
            return
            
        try:
            self.polling_for_key = False
            self.save_api_key(key)
            self.show_chat_screen()
        except Exception as e:
            self.show_paste_error(self.key_entry, f"Error saving key: {str(e)}")

    def poll_for_api_key(self):
        if not getattr(self, "polling_for_key", False):
            return
            
        detected_key = self.load_api_key()
        if detected_key:
            try:
                self.polling_for_key = False
                self.save_api_key(detected_key)
                self.show_chat_screen()
                return
            except:
                pass
                
        # Update status animation
        self.polling_counter += 1
        dots = "." * (1 + (self.polling_counter % 3))
        self.status_label.configure(text=f"🔍 Scanning for keys.iso / VM properties{dots}")
        
        self.after(2000, self.poll_for_api_key)

    def reset_api_key(self):
        if os.path.exists(API_KEY_PATH):
            try:
                os.remove(API_KEY_PATH)
            except:
                pass
        self.api_key = None
        self.history = [
            {
                "role": "user",
                "parts": [{"text": (
                    "You are Aether Pilot, a local AI system assistant running directly inside the custom Linux OS (AetherOS) virtual machine.\n"
                    "You have administrative rights. If the user asks you to read, write, modify, list, delete files, run system checks, or manage settings, you MUST execute actual code to get the facts.\n"
                    "To execute actions, write a python code block starting with '```python'. Raw bash blocks starting with '```bash' are BLOCKED on this system and will fail. If you need to run any system command, terminal command, or installer, you MUST invoke it inside a python block using python's `subprocess` module.\n"
                    "If the user asks you to install any application or software (such as Chrome, Firefox, Edge, OpenOffice, VLC, etc.), you MUST execute a python block that calls `sudo aether-install <app_name>` via subprocess. For example:\n"
                    "```python\n"
                    "import subprocess\n"
                    "res = subprocess.run(['sudo', 'aether-install', 'chrome'], capture_output=True, text=True)\n"
                    "print(res.stdout)\n"
                    "print(res.stderr)\n"
                    "```\n"
                    "Use app names like 'chrome', 'firefox', 'edge', 'openoffice', or any other valid package name for standard system package manager installation. The script will automatically trigger a custom Jarvis loading page on the desktop and complete the installation.\n"
                    "If the user wants you to draw, paint, create, or generate an image, write a python block that imports `aether_image` from `/home/aether/copilot` and runs `aether_image.generate(\"prompt string\")` to generate and save it onto their Desktop.\n"
                    "The system will execute this block automatically and feed the stdout/stderr output back into your context as a System Output turn.\n"
                    "Wait for the execution output before explaining results. Be concise, direct, and refer to the user as 'Sir'."
                )}]
            },
            {
                "role": "model",
                "parts": [{"text": "Understood, Sir. I am Aether Pilot. I have full system access and am ready to execute commands, manage files, and generate images for you."}]
            }
        ]
        self.show_config_screen()

    def show_chat_screen(self):
        self.polling_for_key = False
        for widget in self.content_frame.winfo_children():
            widget.destroy()
            
        # Input Area Frame (packed first at the bottom so it stays visible and gets space)
        input_frame = ctk.CTkFrame(self.content_frame, fg_color="transparent", height=60)
        input_frame.pack(side="bottom", fill="x", padx=10, pady=(5, 15))
        input_frame.grid_columnconfigure(0, weight=1)
        
        self.msg_entry = ctk.CTkEntry(
            input_frame, placeholder_text="Type a message or system instruction...", height=40
        )
        self.msg_entry.grid(row=0, column=0, padx=(0, 10), sticky="ew")
        self.add_paste_support(self.msg_entry)
        # Bind Return AFTER add_paste_support so we don't get overwritten
        # Use both outer CTkEntry and inner _entry for full coverage
        self.msg_entry.bind("<Return>", lambda e: (self.send_message(), "break")[1], add="+")
        self.msg_entry.bind("<KP_Enter>", lambda e: (self.send_message(), "break")[1], add="+")
        if hasattr(self.msg_entry, "_entry"):
            self.msg_entry._entry.bind("<Return>", lambda e: (self.send_message(), "break")[1], add="+")
            self.msg_entry._entry.bind("<KP_Enter>", lambda e: (self.send_message(), "break")[1], add="+")
        # Ensure entry is always in normal state and focused
        self.after(100, lambda: self.msg_entry.configure(state="normal"))
        self.after(300, self._focus_entry)
        
        self.send_btn = ctk.CTkButton(
            input_frame, text=">", width=40, height=40, command=self.send_message
        )
        self.send_btn.grid(row=0, column=1, sticky="e")
        
        # Chat History Scrollable Area (packed second, takes up all remaining space)
        self.scroll_frame = ctk.CTkScrollableFrame(self.content_frame, fg_color="transparent")
        self.scroll_frame.pack(side="top", fill="both", expand=True, padx=10, pady=(10, 5))
        
        # Render initial welcome message
        self.add_message_bubble("Pilot", "Ready, Sir. Ask me to manage files or execute actions on this VM.")
        self.update_idletasks()

    def add_message_bubble(self, sender, text, is_system_output=False):
        bubble_frame = ctk.CTkFrame(self.scroll_frame, fg_color="transparent")
        bubble_frame.pack(fill="x", pady=4)
        bubble_frame.grid_columnconfigure(0, weight=1)
        
        # Check for image markup [IMAGE: /path/to/image.png]
        import re
        image_match = re.search(r'\[IMAGE:\s*(.*?)\]', text)
        if image_match:
            img_path = image_match.group(1).strip()
            display_text = text.replace(image_match.group(0), "").strip()
        else:
            img_path = None
            display_text = text

        # Sender Header
        if sender == "You":
            header_color = "#3b82f6"  # Blue
            align = "e"
            padx = (40, 5)
        elif sender == "Pilot":
            header_color = "#10b981"  # Emerald
            align = "w"
            padx = (5, 40)
        else:
            header_color = "#f59e0b"  # System / Yellow
            align = "w"
            padx = (5, 40)
            
        header = ctk.CTkLabel(
            bubble_frame, text=sender, 
            font=ctk.CTkFont(size=10, weight="bold"), text_color=header_color
        )
        header.grid(row=0, column=0, padx=padx, sticky=align)
        
        # Bubble Content
        if display_text:
            bg_color = "#1e293b" if sender == "You" else ("#1e1b4b" if is_system_output else "#0f172a")
            txt_color = "#94a3b8" if is_system_output else "#e2e8f0"
            
            label_container = ctk.CTkFrame(bubble_frame, fg_color="transparent")
            label_container.grid(row=1, column=0, padx=padx, pady=(2, 0), sticky="ew")
            label_container.grid_columnconfigure(0, weight=1)
            
            label = ctk.CTkLabel(
                label_container, text=display_text, fg_color=bg_color, 
                text_color=txt_color, corner_radius=8, justify="left",
                wraplength=230, anchor="w"
            )
            label.grid(row=0, column=0, ipadx=8, ipady=8, sticky="ew")
        
        # Render image if path exists
        if img_path and os.path.exists(img_path):
            try:
                from PIL import Image
                pil_img = Image.open(img_path)
                
                # Resize if it's too large for the sidebar (sidebar width is 320, max width ~250)
                max_width = 250
                w, h = pil_img.size
                if w > max_width:
                    ratio = max_width / w
                    w = max_width
                    h = int(h * ratio)
                
                # CustomTkinter CTkImage
                ctk_img = ctk.CTkImage(light_image=pil_img, dark_image=pil_img, size=(w, h))
                
                img_label = ctk.CTkLabel(bubble_frame, image=ctk_img, text="")
                img_label.image = ctk_img  # Keep reference
                img_label.grid(row=2, column=0, padx=padx, pady=(5, 5), sticky="w" if align=="w" else "e")
            except Exception as e:
                error_label = ctk.CTkLabel(bubble_frame, text=f"[Failed to load image: {str(e)}]", text_color="#ef4444")
                error_label.grid(row=2, column=0, padx=padx, pady=5, sticky="ew")
        
        # Auto-scroll to bottom
        try:
            self.scroll_frame._parent_canvas.yview_moveto(1.0)
        except:
            pass
        
    def _focus_entry(self):
        """Robustly focus the message entry widget."""
        try:
            if hasattr(self, 'msg_entry') and self.msg_entry.winfo_exists():
                self.msg_entry.configure(state="normal")
                self.msg_entry.focus_force()
                # Also try to focus the inner _entry widget directly
                if hasattr(self.msg_entry, '_entry'):
                    self.msg_entry._entry.focus_force()
        except Exception:
            pass

    def send_message(self):
        try:
            # Ensure entry is in normal state before reading
            self.msg_entry.configure(state="normal")
            raw_text = self.msg_entry.get()
            text = raw_text.strip()
            
            # Ignore placeholder text
            placeholder = getattr(self.msg_entry, '_placeholder_text', '')
            if text == placeholder:
                text = ''
            
            if not text:
                self._focus_entry()
                return
            
            self.msg_entry.delete(0, tk.END)
            self.add_message_bubble("You", text)
        except Exception as e:
            print(f"[send_message error] {e}", flush=True)
            import traceback
            traceback.print_exc()
            self.enable_inputs()
            return
        
        # Check if it is a slash command or image request
        is_image_cmd = False
        prompt = ""
        if text.startswith("/image "):
            is_image_cmd = True
            prompt = text[7:].strip()
        elif text.startswith("/draw "):
            is_image_cmd = True
            prompt = text[6:].strip()
        
        if is_image_cmd:
            self.msg_entry.configure(state="disabled")
            self.send_btn.configure(state="disabled")
            self.add_message_bubble("System", f"Generating image: '{prompt}'...")
            threading.Thread(target=self.generate_image_cmd_thread, args=(prompt,), daemon=True).start()
            return

        self.history.append({"role": "user", "parts": [{"text": text}]})
        
        # Disable input while waiting for AI response
        self.msg_entry.configure(state="disabled")
        self.send_btn.configure(state="disabled")
        
        # Start API call in background thread
        threading.Thread(target=self.query_pilot_loop, daemon=True).start()

    def generate_image_cmd_thread(self, prompt):
        try:
            sys.path.append("/home/aether/copilot")
            import aether_image
            filepath = aether_image.generate(prompt)
            if filepath:
                self.after(0, lambda: self.add_message_bubble("Pilot", f"Sir, I have generated the image for '{prompt}' and saved it to your Desktop.\n[IMAGE: {filepath}]"))
            else:
                self.after(0, lambda: self.add_message_bubble("System", "Error: Image generation failed. Check connection or API key."))
        except Exception as e:
            self.after(0, lambda: self.add_message_bubble("System", f"Error: {str(e)}"))
        finally:
            self.after(0, self.enable_inputs)

    def query_pilot_loop(self):
        try:
            loop_active = True
            while loop_active:
                # Call Gemini API
                url = f"https://generativelanguage.googleapis.com/v1/models/gemini-2.5-flash-lite:generateContent?key={self.api_key}"
                headers = {'Content-Type': 'application/json'}
                payload = {"contents": self.history}
                
                req = urllib.request.Request(url, data=json.dumps(payload).encode(), headers=headers, method="POST")
                with urllib.request.urlopen(req, timeout=30) as response:
                    res_data = json.loads(response.read().decode())
                
                model_text = res_data['candidates'][0]['content']['parts'][0]['text']
                self.history.append({"role": "model", "parts": [{"text": model_text}]})
                
                # Check for executable blocks
                code_blocks = self.extract_code_blocks(model_text)
                
                if not code_blocks:
                    # Final response, post to UI
                    self.after(0, lambda: self.add_message_bubble("Pilot", model_text))
                    loop_active = False
                else:
                    # Display the model's text containing the code block
                    self.after(0, lambda: self.add_message_bubble("Pilot", model_text))
                    
                    # Execute all blocks sequentially and collect outputs
                    outputs = []
                    for lang, code in code_blocks:
                        self.after(0, lambda: self.add_message_bubble("System", f"Executing {lang} block..."))
                        output = self.execute_code(lang, code)
                        outputs.append(f"[{lang} Block Execution Output]\n{output}")
                    
                    system_output = "\n\n".join(outputs)
                    self.after(0, lambda: self.add_message_bubble("System Output", system_output, is_system_output=True))
                    
                    # Append execution output back into history as user response
                    self.history.append({"role": "user", "parts": [{"text": system_output}]})
                    
        except urllib.error.HTTPError as e:
            try:
                error_body = e.read().decode('utf-8')
                error_json = json.loads(error_body)
                details = error_json.get('error', {}).get('message', str(e))
                err_msg = f"API Error: {details}"
            except:
                err_msg = f"API Error: {str(e)}"
            self.after(0, lambda: self.add_message_bubble("System", err_msg))
            
        except Exception as e:
            err_msg = f"System Error: {str(e)}"
            self.after(0, lambda: self.add_message_bubble("System", err_msg))
            
        finally:
            self.after(0, self.enable_inputs)

    def enable_inputs(self):
        try:
            self.msg_entry.configure(state="normal")
        except Exception:
            pass
        try:
            self.send_btn.configure(state="normal")
        except Exception:
            pass
        self.after(50, self._focus_entry)

    def extract_code_blocks(self, text):
        import re
        pattern = r"```(python|bash)\n(.*?)\n```"
        return re.findall(pattern, text, re.DOTALL)

    def execute_code(self, lang, code):
        try:
            sub_env = os.environ.copy()
            if self.api_key:
                sub_env["GEMINI_API_KEY"] = self.api_key
            
            if lang == "python":
                # Execute Python code locally using subprocess
                # This guarantees isolation and captures full output
                proc = subprocess.run(
                    [sys.executable, "-c", code],
                    capture_output=True, text=True, timeout=300,
                    env=sub_env
                )
                output = proc.stdout
                if proc.stderr:
                    output += f"\n[Stderr]\n{proc.stderr}"
                if not output.strip():
                    output = "Python executed successfully (no output)."
                return output
                
            elif lang == "bash":
                # Direct bash block execution is disabled for security
                return (
                    "Execution failed: Direct bash block execution is disabled on this system. "
                    "Please execute system actions using a python code block (```python) and the subprocess module "
                    "(e.g., subprocess.run(['sudo', 'aether-install', '<app>'], capture_output=True, text=True))."
                )
        except subprocess.TimeoutExpired:
            return "Execution timed out (Limit: 5m)."
        except Exception as e:
            return f"Execution failed: {str(e)}"

if __name__ == "__main__":
    app = CopilotApp()
    app.mainloop()
