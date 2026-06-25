#!/usr/bin/env python3
import os
os.environ['DISPLAY'] = ':0'
os.environ['HOME'] = '/home/aether'

import customtkinter as ctk
import tkinter as tk

root = ctk.CTk()
e = ctk.CTkEntry(root, placeholder_text="test")
e.pack()
root.update()

# Check inner widget
print("Has _entry:", hasattr(e, '_entry'))
if hasattr(e, '_entry'):
    print("_entry type:", type(e._entry).__name__)
    # Check what bindings exist
    bindings = e._entry.bind()
    print("_entry bindings:", bindings)

# Check outer bindings
outer_bindings = e.bind()
print("outer CTkEntry bindings:", outer_bindings)

# Check if event propagation works
result = []
def on_return(event):
    result.append("RETURN_FIRED")
    
e._entry.bind('<Return>', on_return) if hasattr(e, '_entry') else e.bind('<Return>', on_return)

# Simulate keypress
root.after(100, lambda: e._entry.event_generate('<Return>') if hasattr(e, '_entry') else e.event_generate('<Return>'))
root.after(500, root.destroy)
root.mainloop()

print("Result:", result)
