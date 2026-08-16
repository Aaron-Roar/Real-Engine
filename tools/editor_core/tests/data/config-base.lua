local configure_program = "base-configure"

return {
    editor = {
        font = "/fonts/base.ttf",
    },
    project = {
        configure = { configure_program, "{project}", "{build}" },
        compile = { "base-compile", "{build}" },
    },
    cli = {
        compile = { "cli-compile", "{sdk}" },
    },
}
