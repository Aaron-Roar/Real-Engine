-- Rohr SDK editor defaults.
-- Prefer shared project commands. Frontend overrides are available for
-- advanced cases where the CLI and GUI genuinely require different tools.
return {
    editor = {
        config_path_override = nil,
        gui_state_path_override = nil,
        font = nil,

        window = {
            mode = "windowed",
            aspect_ratio = "auto",
            resolution = { 1280, 720 },
        },

        terminal = {
            visible = true,
            height_ratio = 0.125,
        },
    },

    build = {
        project = {
            configure = {
                "cmake", "-S", "{project}", "-B", "{build}",
                "-DROHR_ENGINE_SOURCE_ROOT={sdk}",
            },
            compile = { "cmake", "--build", "{build}" },
        },
        cli = {
            configure = nil,
            compile = nil,
        },
        gui = {
            configure = nil,
            compile = nil,
        },
    },
}
