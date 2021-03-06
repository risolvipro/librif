import "CoreLibs/crank"
import "librif"

local gfx = playdate.graphics
local display = playdate.display

local image = librif.image.open("images/track-512-raw.rif")
image:read()

local scale = 1
local minScale = display:getWidth() / image:getWidth()
local maxScale = 8

local offset = {
    x = 0,
    y = 0
}

local move = 60
local crankMultiplier = 0.01

local firstChange = true

local math_floor = math.floor
local math_min = math.min
local math_max = math.max

local round = function(n)
    return math_floor(n + 0.5)
end

function librif_test.update()

    -- save old state

    local oldOffset = {
        x = offset.x,
        y = offset.y
    }

    local oldScale = scale

    -- input

    if playdate.buttonJustPressed(playdate.kButtonLeft) then
        offset.x = offset.x - move
    elseif playdate.buttonJustPressed(playdate.kButtonUp) then
        offset.y = offset.y - move
    elseif playdate.buttonJustPressed(playdate.kButtonRight) then
        offset.x = offset.x + move
    elseif playdate.buttonJustPressed(playdate.kButtonDown) then
        offset.y = offset.y + move
    end

    scale = scale + playdate.getCrankChange() * crankMultiplier

    -- clamp values

    scale = math_min(maxScale, scale)
    scale = math_max(minScale, scale)

    local oldScaledWidth = round(image:getWidth() * oldScale)
    local oldScaledHeight = round(image:getHeight() * oldScale)

    local scaledWidth = round(image:getWidth() * scale)
    local scaledHeight = round(image:getHeight() * scale)

    -- center zoom

    offset.x = (offset.x + display.getWidth() / 2) / oldScaledWidth * scaledWidth -  display.getWidth() / 2
    offset.y = (offset.y + display.getHeight() / 2) / oldScaledHeight * scaledHeight -  display.getHeight() / 2

    -- clamp values

    offset.x = math_min(offset.x, scaledWidth - display.getWidth())
    offset.y = math_min(offset.y, scaledHeight - display.getHeight())

    offset.x = math_max(0, offset.x)
    offset.y = math_max(0, offset.y)

    local needsDisplay = false

    if firstChange then
        needsDisplay = true
        firstChange = false
    elseif oldOffset.x ~= offset.x or oldOffset.y ~= offset.y then
        needsDisplay = true
    elseif oldScale ~= scale then
        needsDisplay = true
    end

    if needsDisplay then
        image:setSize(scaledWidth, scaledHeight)
        image:setPosition(-offset.x, -offset.y)
        image:draw()
    end

    playdate.drawFPS(0, 0)
end