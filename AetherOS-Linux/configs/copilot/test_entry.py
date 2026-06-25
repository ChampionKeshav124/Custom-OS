import tkinter as tk
root = tk.Tk()
root.title("TEST ENTRY")
root.geometry("300x100")
e = tk.Entry(root)
e.pack(pady=20)
e.focus()
root.mainloop()
