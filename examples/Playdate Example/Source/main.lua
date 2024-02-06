local gfx = playdate.graphics
local display = playdate.display

local image = librif.image.open("images/track-1024-raw.rif")
image:read()

local color, alpha = image:getPixel(0, 0)
print(color, alpha)

function playdate.update()
    
end