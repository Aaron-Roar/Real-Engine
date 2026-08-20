-- Copyright 2026 Aaron Rohrer
-- SPDX-License-Identifier: LGPL-3.0-only

-- Project-local Rohr editor configuration.
-- Missing values inherit the SDK configuration and then built-in defaults.
return {
    build = {
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
    },
}
