/* ==============================================================
   AETHEROS — BUILD YOUR OWN OPERATING SYSTEM FROM SCRATCH
   Interactive Shell v1 Header (AetherOS-64)
   ============================================================== */

#ifndef SHELL_H
#define SHELL_H

/* shell_init: Initializes the shell, clears the buffer, and prints
   the initial shell prompt string. */
void shell_init(void);

/* shell_input_char: Feeds a typed ASCII character to the shell.
   Handles command execution on '\n' (Enter) and character deletion
   on '\b' (Backspace). */
void shell_input_char(char c);

#endif /* SHELL_H */
