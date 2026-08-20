-- Copyright 2026 Aaron Rohrer
-- SPDX-License-Identifier: LGPL-3.0-only

local configure_program = "base-configure"

return {
    editor = {
        font = "/fonts/base.ttf",
        config_path_override = "/config/editor.lua",
        gui_state_path_override = "/config/editor_gui_state.lua",
    },
    build = {
        project = {
            configure = { configure_program, "{project}", "{build}" },
            compile = { "base-compile", "{build}" },
        },
        cli = {
            compile = { "cli-compile", "{sdk}" },
        },
    },
}
