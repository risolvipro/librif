## librif

A lightweight library to encode and read grayscale images, with Playdate support.

Librif stores images in raw binary data (default setting) or in a compressed format. If compression is enabled, the encoder uses a simple RLE algorithm which finds repeating patterns in the image. Librif is optimized to minimize RAM usage at runtime.

## Table of contents

- [Setup](#setup)
- [Encoder](#encoder)
- [Sample image](#sample-image)
- [Playdate support](#playdate-support)
- [C Library](#c-library)
- [Pool](#pool)
- [Lua for Playdate](#lua-for-playdate)
- [Graphics on Playdate](#graphics-on-playdate)
- [Format specification](#format-specification)

## Setup

Install Pillow `pip install Pillow` (required for encoder)

## Encoder

The Python encoder supports the following commands:

* `-i` `--input` Input file (*optional*). If no input file is passed the enconder will look for *src* and *dst* folders in the working directory
* `-c` `--compress` Enable compression algorithm
* `-pmin` `--pattern-min` Set the minimum pattern size for compression (default **8**)
* `-pmax` `--pattern-max` Set the maximum pattern size for compression (default **8**)
* `-pstep` `--pattern-step` Set the step used to find the pattern (default **2**)
* `--png-output` Save input image as png output
* `-v` `--verbose` Enable verbose mode

### Usage

`python encoder.py -i image.png` (uncompressed)\
`python encoder.py -i image.png -c` (compressed)

## Sample image

<p>
<img src="images/track-1024.png?raw=true" width="300">
</p>

1024 x 1024px

Raw image (grayscale) | PNG (RGB) | RIF compressed (grayscale)
|---|---|---|
| 1 MB | 34 KB (96.6%) | 77 KB (92.3%) |

## Playdate support

Supporting Playdate:

* Add the preprocessor flag `PLAYDATE=1` in your editor
* Change the Makefile as follows

```makefile
SRC += librif.c

# lua support
SRC += src/librif_luaglue.c

UDEFS = -DPLAYDATE=1

CLANGFLAGS = -DPLAYDATE=1
```

Sample code

```c
#include "librif.h"

// set PlaydateAPI
RIF_pd = pd
```

## C Library

The C library has two functions to open and read an image.

```c
nullable RIF_Image* librif_image_open(const char *filename, nullable RIF_Pool *pool);
bool librif_image_read(RIF_Image *image, size_t size, nullable bool *closed);
```

The compressed image has corresponding functions.

```c
nullable RIF_CImage* librif_cimage_open(const char *filename, nullable RIF_Pool *pool);
bool librif_cimage_read(RIF_CImage *image, size_t size, nullable bool *closed);
```

Example
```c
RIF_Image *image = librif_image_open("image.rif", NULL);
if(image != NULL){

    while(1){
        bool closed;
        bool success = librif_image_read(image, (50 * 1000), &closed);
        if(!success || closed){
            break;
        }
    }
}
```

Getting a pixel from image.

```c
uint8_t color, alpha;
librif_image_get_pixel(image, &color, &alpha);
```

You can also get a pixel from `pixels` to skip some security checks.

```c
uint8_t color = image->pixels[y * image->width + x]
```

```c
RIF_Pixel pixel = image->pixels_a[y * image->width + x]
```

### Notes

* In `librif_image_read`, pass `0` size to read the entire file
* Image properties: `hasAlpha`, `width`, `height`
* Properties `readBytes` and `totalBytes` can be used to track loading, please note that they don't reflect actual bytes.

## Pool

Images are saved in memory with malloc, you can optionally create a `RIF_Pool` instance to load them into reserved memory.

```c
// 1 MB pool
RIF_Pool *pool = librif_pool_new(1000 * 1000);

// open an image passing pool as 2nd argument
RIF_Image *image = librif_image_open("image.rif", pool);

// clear pool restoring original start address
librif_pool_clear(pool);

// free pool later
librif_pool_free(pool);
```

## Lua for Playdate

### C Setup

```c
// include librif and lua support
#include "librif_luaglue.h"

// set PlaydateAPI
RIF_pd = pd

// register lua classes
librif_lua_register();
```

### Lua setup

```lua
import "librif"

-- init graphics (recommended)
-- graphics has a lazy loading mechanism, you may want to call it manually
-- calling graphics.init() is not required if you don't use graphics
librif.graphics.init()
```

In Lua there are two image objects `librif.image` and `librif.cimage` that share the same methods.

* `image.open(filename, [pool])` to open an image
* `image:read(size)` to read the image, returns a tuple `(success, closed)`
* `image:hasAlpha()`
* `image:getWidth()`
* `image:getHeight()`
* `image:getReadBytes()`
* `image:getTotalBytes()`

`librif.cimage` object

* `cimage:decompress([pool])` decompress a cimage returning an image object

`librif.pool` object

* `pool.new(size)`
* `pool:clear()`
* `pool:release()`

You should call `pool:release()` to let Lua Garbage Collector release the object.

Example
```lua
local image = librif.image.open("image.rif")

while true do
    local success, closed = image:read(1000)
    if closed then
        break
    end
end
```

## Graphics on Playdate

On Playdate, you can draw images with

* `image:draw(x, y)`
* `image:drawScaled(x, y, width, height)`

Additional methods

* `graphics.setDitherType(type)`
* `graphics.setBlendColor(color)` set a blend color that will be ignored in drawing. Use it to get transparency on non-alpha images.
* `graphics.clearBlendColor()` clear the blend color

Constants

* `graphics.kDitherTypeBayer2`
* `graphics.kDitherTypeBayer4`
* `graphics.kDitherTypeBayer8`

## Format specification

Format specification is subject to changes.

First bytes of the file are metadata.

* 1 byte (uint8) for alpha support
* 4 byte (uint32) image width
* 4 byte (uint32) image height

Pixels are stored in a rows / columns layout.

### In raw mode

Non-alpha: 1 byte per pixel (uint8)

Alpha: 1 byte for alpha (uint8) and 1 byte for color (uint8). Color is skipped if alpha equals to zero

### In compressed mode

Additional metadata.

* 4 byte (uint32) number of cells columns
* 4 byte (uint32) number of cells rows
* 4 byte (uint32) pattern size
* 4 byte (uint32) number of patterns

Patterns data

Pattern pixels are stored in "raw mode".

Cells data

Pattern indexes (0-based) as 4 byte uint32.