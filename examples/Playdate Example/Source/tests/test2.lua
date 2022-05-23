import "librif"

local gfx = playdate.graphics
local display = playdate.display

local image = librif.image.open("images/mario-raw.rif")
image:read()

local math_floor = math.floor

local round = function(n)
    return math_floor(n + 0.5)
end

local imageLoaded = false
local transformedImage = nil
local rotation = 0

local kColorWhite = playdate.graphics.kColorWhite
local drawBounds = table.pack(0, 0, 0, 0)

function librif_test.update()
    
    local height = 160
    local width = round(height * image:getWidth() / image:getHeight())

    local cx = display:getWidth() / 2
    local cy = display:getHeight() / 2

    gfx.setColor(kColorWhite)

    local bx, by, bw, bh = table.unpack(drawBounds)
    gfx.fillRect(bx, by, bw, bh)

    local dt = playdate.getElapsedTime()
    playdate.resetElapsedTime()

    rotation = rotation + 80 * dt
    
    if rotation > 360 then
        rotation = rotation % 360
    end

    image:setSize(width, height)
    image:setCenter(0.5, 0.5)
    image:setRotation(rotation)
    image:setPosition(cx, cy)
    image:draw()

    drawBounds = table.pack(librif.graphics.getDrawBounds())

    playdate.drawFPS(0, 0)
end