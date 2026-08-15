# Rohr Terminal

Rohr Terminal is an optional tool library for applications such as the Rohr
Editor. It does not depend on the engine or change the caller's working
directory.

The caller owns the `RohrTerminal` returned by `rohr_terminal_create()` and must
release it with `rohr_terminal_destroy()`. `rohr_terminal_update()` performs
non-blocking output collection and should be called once per application frame.
Line views remain valid until the next update or destruction.

The Linux backend runs the configured shell through a PTY. The Windows backend
runs `%COMSPEC%` (normally `cmd.exe`) through ConPTY and therefore requires
Windows 10 version 1809 or newer. Only the child shell uses `working_directory`;
all editor paths remain explicit on both platforms.

Current terminal interpretation supports UTF-8, bounded line scrollback, basic
SGR colors and attributes, command input, and Ctrl+C. Full cursor-addressed and
full-screen terminal applications are intentionally deferred.
