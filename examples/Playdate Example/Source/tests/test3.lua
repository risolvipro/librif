import "librif"
import "CoreLibs/sprites"

local gfx = playdate.graphics
local display = playdate.display

local image = librif.image.open("images/mario-raw.rif")
image:read()

local bitmap = image:toBitmap()

local sprite = playdate.graphics.sprite.new(bitmap)
sprite:add()

local x = 0
local regular_acceleration = 60
local acceleration = regular_acceleration

function librif_test.update()
    
    local dt = playdate.getElapsedTime()
    playdate.resetElapsedTime()

    local proposed_x = x + acceleration * dt

    if proposed_x < 0 then
        acceleration = regular_acceleration
    elseif (proposed_x + sprite.width) > display.getWidth() then
        acceleration = - regular_acceleration
    end

    x = proposed_x
    x = math.min(x, display.getWidth() - sprite.width)
    x = math.max(0, x)

    local y = display:getHeight() / 2

    sprite:moveTo(x + sprite.width / 2, y)

    playdate.graphics.sprite.update()

    playdate.drawFPS(0, 0)
end