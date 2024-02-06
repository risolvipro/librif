-- Globals provided by librif.
--
-- This file can be used by toyboypy (https://toyboxpy.io) to import into a project's luacheck config.
--
-- Just add this to your project's .luacheckrc:
--    require "toyboxes/luacheck" (stds, files)
--
-- and then add 'toyboxes' to your std:
--    std = "lua54+playdate+toyboxes"

return {
    globals = {
        librif = {
            fields = {
                image = {
                    fields = {
                        open = {},
                        read = {},
                        getWidth = {},
                        getHeight = {},
                        hasAlpha = {},
                        getPixel = {},
                        setPixel = {},
                        getReadBytes = {},
                        getTotalBytes = {},
                        copy = {}
                    }
                },
                cimage = {
                    fields = {
                        open = {},
                        read = {},
                        getWidth = {},
                        getHeight = {},
                        hasAlpha = {},
                        getPixel = {},
                        getReadBytes = {},
                        getTotalBytes = {},
                        decompress = {}
                    }
                },
                pool = {
                    fields = {
                        new = {},
                        realloc = {},
                        clear = {},
                        release = {}
                    }
                }
            }
        }
    }
}
