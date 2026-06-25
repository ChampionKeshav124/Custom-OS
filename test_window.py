import customtkinter as ctk
import sys

app = ctk.CTk()
app.geometry("300x400+100+100")

# Test 'utility' type
app.attributes("-type", "utility")
app.attributes("-topmost", True)

lbl = ctk.CTkLabel(app, text="This is utility type")
lbl.pack(pady=20)

entry = ctk.CTkEntry(app)
entry.pack(pady=20)
entry.focus()

# Exit after 10 seconds so it doesn't hang forever
app.after(10000, app.destroy)
app.mainloop()
