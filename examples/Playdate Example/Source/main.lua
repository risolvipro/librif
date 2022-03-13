import "CoreLibs/crank"
import "pattern4"

local gfx = playdate.graphics
local display = playdate.display

-- 500 KB pool
local pool = librif.pool.new(500 * 1000)
local image = librif.cimage.open("track-compressed.rif", pool)

local scale = 1
local minScale = display:getWidth() / image:getWidth()
local maxScale = 6

local offset = {
    x = 0,
    y = 0
}

local move = 60
local crankMultiplier = 0.01

local kWhite = gfx.kColorWhite
local kBlack = gfx.kColorBlack

local firstChange = true

local math_floor = math.floor
local math_min = math.min
local math_max = math.max

local round = function(n)
    return math_floor(n + 0.5)
end

local imageLoaded = false

display.setRefreshRate(0)

function playdate.update()

    if (not imageLoaded) then
        -- read image in chunks of 1000 bytes
        local success, closed = image:read(1000)
        if closed then
            imageLoaded = true
            display.setRefreshRate(15)
        end
    else
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

        local oldScaledWidth = image:getWidth() * oldScale
        local oldScaledHeight = image:getHeight() * oldScale

        local scaledWidth = image:getWidth() * scale
        local scaledHeight = image:getHeight() * scale

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
        elseif ((not (oldOffset.x == offset.x)) or (not (oldOffset.y == offset.y))) then
            needsDisplay = true
        elseif (not (oldScale == scale)) then
            needsDisplay = true
        end

        if needsDisplay then
            local visibleWidth = math_min(scaledWidth, display.getWidth())
            local visibleHeight = math_min(scaledHeight, display.getHeight())

            local x, y
            local scaledX, scaledY
            local d_col, d_row
            local color, alpha

            local relY
            for relY = 0, visibleHeight do
                y = relY
                
                d_row = y % 4
                scaledY = round((relY + offset.y) / scale)

                local relX
                for relX = 0, visibleWidth do
                    x = relX
                    scaledX = round((relX + offset.x) / scale)
                    
                    color, alpha = image:getPixel(scaledX, scaledY)

                    d_col = x % 4

                    ditherColor = kWhite
                    if (color < pattern4[d_col][d_row]) then
                        ditherColor = kBlack
                    end

                    gfx.setColor(ditherColor)
                    gfx.drawPixel(x, y)
                end
            end
        end
    end

    playdate.drawFPS(0, 0)
end
