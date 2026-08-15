# Rohr Terminal

Rohr Terminal is an optional tool library for applications such as the Rohr
Editor. It does not depend on the engine or change the caller's working
directory.

The caller owns the `RohrTerminal` returned by `rohr_terminal_create()` and must
release it with `rohr_terminal_destroy()`. `rohr_terminal_update()` performs
non-blocking output collection and should be called once per application frame.
Line views remain valid until the next update or destruction.

The Linux backend runs the configured shell through a PTY. The child shell alone
uses `working_directory`; all editor paths remain explicit. Windows currently
builds an unsupported-platform backend pending ConPTY support.

Current terminal interpretation supports UTF-8, bounded line scrollback, basic
SGR colors and attributes, command input, and Ctrl+C. Full cursor-addressed and
full-screen terminal applications are intentionally deferred.
