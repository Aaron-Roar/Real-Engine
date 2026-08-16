-- Rohr SDK editor defaults.
-- Prefer shared project commands. Frontend overrides are available for
-- advanced cases where the CLI and GUI genuinely require different tools.
return {
    editor = {
        font = nil,
    },

    project = {
        configure = nil,
        compile = nil,
    },

    cli = {
        configure = nil,
        compile = nil,
    },

    gui = {
        configure = nil,
        compile = nil,
    },
}
