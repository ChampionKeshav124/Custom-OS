#!/usr/bin/env python3
# ==============================================================
# AETHEROS LINUX — CLASSIC HOBBY KERNEL SHELL EMULATOR
# Replicates the look, feel, and commands of AetherOS-64 Milestone 7.
# ==============================================================

import os
import sys
import subprocess
import time

# VGA Text Colors in ANSI escape codes
VGA_WHITE = "\033[37m"
VGA_LIGHT_CYAN = "\033[96m"
VGA_LIGHT_GREEN = "\033[92m"
VGA_LIGHT_MAGENTA = "\033[95m"
VGA_LIGHT_RED = "\033[91m"
RESET = "\033[0m"

def print_prompt():
    print(f"{VGA_LIGHT_CYAN}AetherOS>{RESET} ", end="", flush=True)

def shell_execute(cmd, args):
    cmd = cmd.strip()
    args = args.strip()

    if cmd == "help":
        print(f"{VGA_LIGHT_CYAN}AetherOS-64 Shell Help Menu:{RESET}")
        print(f"  {VGA_LIGHT_MAGENTA}help{RESET}          - Display this help information")
        print(f"  {VGA_LIGHT_MAGENTA}about{RESET}         - Show operating system details")
        print(f"  {VGA_LIGHT_MAGENTA}version{RESET}       - Print OS version information")
        print(f"  {VGA_LIGHT_MAGENTA}meminfo{RESET}       - Display physical memory allocation stats")
        print(f"  {VGA_LIGHT_MAGENTA}fault-div{RESET}     - Trigger CPU Division by Zero Exception (#DE)")
        print(f"  {VGA_LIGHT_MAGENTA}fault-pf{RESET}      - Trigger CPU Page Fault Exception (#PF)")
        print(f"  {VGA_LIGHT_MAGENTA}echo [text]{RESET}   - Print the text argument to screen")
        print(f"  {VGA_LIGHT_MAGENTA}clear{RESET}         - Clear the screen buffer")
        print(f"  {VGA_LIGHT_MAGENTA}reboot{RESET}        - Hard restart the computer")
    
    elif cmd == "about":
        print(f"{VGA_LIGHT_GREEN}AetherOS v0.1.0-alpha{RESET}")
        print(f"Architecture: {VGA_LIGHT_MAGENTA}x86_64 Long Mode{RESET}")
        print(f"Created by   : {VGA_LIGHT_MAGENTA}Expert OS Engineer & Beginner Mentee{RESET}")
        print(f"Description  : {VGA_LIGHT_MAGENTA}Written from scratch in assembly and C!{RESET}")
        
    elif cmd == "version":
        print(f"{VGA_LIGHT_CYAN}AetherOS v0.1.0-alpha (64-bit Long Mode | El Torito CD-ROM ISO){RESET}")
        
    elif cmd == "meminfo":
        print(f"{VGA_LIGHT_CYAN}AetherOS-64 PMM Memory Info:{RESET}")
        print(f"  {VGA_LIGHT_MAGENTA}Total RAM:{RESET} {VGA_WHITE}2048{RESET} {VGA_LIGHT_MAGENTA}MB ({RESET}{VGA_WHITE}524288{RESET}{VGA_LIGHT_MAGENTA} pages){RESET}")
        print(f"  {VGA_LIGHT_MAGENTA}Used RAM:{RESET}  {VGA_WHITE}48192{RESET} {VGA_LIGHT_MAGENTA}KB ({RESET}{VGA_WHITE}12048{RESET}{VGA_LIGHT_MAGENTA} pages){RESET}")
        print(f"  {VGA_LIGHT_MAGENTA}Free RAM:{RESET}  {VGA_WHITE}2000{RESET} {VGA_LIGHT_MAGENTA}MB ({RESET}{VGA_WHITE}512240{RESET}{VGA_LIGHT_MAGENTA} pages){RESET}")
        
    elif cmd == "fault-div":
        print(f"{VGA_LIGHT_RED}Triggering Division by Zero Exception...{RESET}")
        time.sleep(1)
        try:
            val = 5 / 0
        except ZeroDivisionError as e:
            print(f"{VGA_LIGHT_RED}CPU Exception #DE: Division by Zero detected.{RESET}")
            
    elif cmd == "fault-pf":
        print(f"{VGA_LIGHT_RED}Triggering Page Fault Exception...{RESET}")
        time.sleep(1)
        print(f"{VGA_LIGHT_RED}Segmentation fault (core dumped){RESET}")
        
    elif cmd == "echo":
        print(f"{VGA_WHITE}{args}{RESET}")
        
    elif cmd == "clear":
        os.system("clear")
        
    elif cmd == "reboot":
        print(f"{VGA_LIGHT_RED}Rebooting system...{RESET}")
        time.sleep(1)
        subprocess.run(["sudo", "reboot"])
        
    else:
        print(f"{VGA_LIGHT_RED}Command not found: {cmd}{RESET}")

def main():
    os.system("clear")
    print(f"{VGA_LIGHT_GREEN}=============================================================={RESET}")
    print(f"{VGA_LIGHT_GREEN}        Welcome to AetherOS Classic Hobby Kernel Shell        {RESET}")
    print(f"{VGA_LIGHT_GREEN}=============================================================={RESET}")
    print("Type 'help' for a list of available commands.\n")

    while True:
        try:
            print_prompt()
            user_input = sys.stdin.readline()
            if not user_input:
                break # EOF
            
            user_input = user_input.strip()
            if not user_input:
                continue
                
            # Tokenize command and args
            space_idx = user_input.find(" ")
            if space_idx != -1:
                cmd = user_input[:space_idx]
                args = user_input[space_idx + 1:]
            else:
                cmd = user_input
                args = ""
                
            shell_execute(cmd, args)
        except KeyboardInterrupt:
            print("\nUse exit command or Ctrl+D to close classic shell.")
        except Exception as e:
            print(f"Shell Error: {e}")

if __name__ == "__main__":
    main()
