-- Copyright 2026 Aaron Rohrer
-- SPDX-License-Identifier: LGPL-3.0-only

return {
    build = {
        project = {
            configure = { "project-configure", "{project}" },
        },
        cli = {
            compile = { "project-cli-compile", "{build}" },
        },
    },
}
