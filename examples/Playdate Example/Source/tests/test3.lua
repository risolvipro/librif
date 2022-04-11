import "librif"
import "CoreLibs/sprites"

local gfx = playdate.graphics
local display = playdate.display

local image = librif.image.open("images/mario.rif")

local imageLoaded = false
local sprite = nil

local x = 0
local acceleration = 60

display.setRefreshRate(0)

function librif_test.update()

    if not imageLoaded then
        -- read image in chunks of 10 KB
        local success, closed = image:read(10 * 1000)
        if closed then
            imageLoaded = true
            display.setRefreshRate(30)
            
            local bitmap = image:toBitmap()

            sprite = playdate.graphics.sprite.new(bitmap)
            sprite:add()
        end
    else
        local dt = playdate.getElapsedTime()
        playdate.resetElapsedTime()

        local proposed_x = x + acceleration * dt

        if proposed_x < 0 or (proposed_x + sprite.width) > display.getWidth()  then
            acceleration = - acceleration
        end

        x = math.min(proposed_x, display.getWidth() - sprite.width)
        x = math.max(0, proposed_x)

        local y = display:getHeight() / 2

        sprite:moveTo(x + sprite.width / 2, y)

        playdate.graphics.sprite.update()
    end

    playdate.drawFPS(0, 0)
end