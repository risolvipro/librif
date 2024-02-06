## librif

[![MIT License](https://img.shields.io/github/license/risolvipro/librif)](https://spdx.org/licenses/MIT.html) [![Lua Version](https://img.shields.io/badge/Lua-5.4-yellowgreen)](https://lua.org) [![Toybox Compatible](https://img.shields.io/badge/toybox.py-compatible-brightgreen)](https://toyboxpy.io)

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
* `--png` Save input image as png output
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

## Playdate support (using toybox.py)

You can add it to your **Playdate** project by installing [**toybox.py**](https://toyboxpy.io), going to your project folder in a Terminal window and typing:

```console
toybox add librif
toybox update
```

Then add `toyboxes/toybox.mk` to your existing makefile or use the `toybox setupMakefile` command to generate one.

## Playdate support (manually)

Change the Makefile as follows

```makefile
SRC += librif.c

# lua support
SRC += src/librif_luaglue.c
```

Sample code

```c
#include "librif.h"

// set PlaydateAPI
librif_init(playdate);
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
        bool success = librif_image_read(image, (10 * 1000), &closed);
        if(!success || closed){
            break;
        }
    }
}
```

Getting a pixel.

```c
uint8_t color, alpha;
librif_image_get_pixel(image, x, y, &color, &alpha);
```

You can also get a pixel from `pixels` to skip some security checks.

```c
uint8_t color = image->pixels[y * image->width + x]
```

Setting a pixel.

```c
librif_image_set_pixel(image, x, y, 0, 255);
```

### Notes

* `librif_image_read`, pass `0` size to read the entire file
* Image properties: `hasAlpha`, `width`, `height`
* Properties `readBytes` and `totalBytes` can be used to track loading

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
librif_init(playdate);

// register lua classes
librif_register_lua();
```

### Lua

Example
```lua
local image = librif.image.open("image.rif")
image:read()
```

In Lua there are two image objects `librif.image` and `librif.cimage` that share the same methods.

* `image.open(filename, [pool])` open an image
* `image:read([size])` read the image, returns a tuple `(success, closed)`
* `image:getPixel(x, y)` get the pixel at x, y as a tuple `(color, alpha)`
* `image:hasAlpha()`
* `image:getWidth()`
* `image:getHeight()`
* `image:getReadBytes()`
* `image:getTotalBytes()`

`librif.image` object

* `image:setPixel(x, y, color, alpha)` set the pixel at x, y
* `image:copy()` returns a copy of the image

`librif.cimage` object

* `cimage:decompress([pool])` decompress a cimage returning an image object

`librif.pool` object

* `pool.new(size)`
* `pool:realloc(size)`
* `pool:clear()`
* `pool:release()`

You should call `pool:release()` to let Lua Garbage Collector release the object.

## Format specification

Format specification is subject to changes.

### Metadata

| Type | Detail |
|:---|:---|
| `uint8` | Alpha support |
| `uint32` | Image width |
| `uint32` | Image height |

### Pixel format

| Type | Detail |
|:---|:---|
| Non-alpha | 1 byte per pixel |
| Alpha | 2 bytes per pixel. uint8 for color, uint8 for alpha |

### In Raw mode

| Size | Detail |
|:---|:---|
| n_pixels * pixel_size | Pixels stored in a rows / columns layout |

### In Compressed mode

Additional metadata.

| Type | Detail |
|:---|:---|
| `uint32` | Number of cells columns |
| `uint32` | Number of cells rows |
| `uint32` | Pattern size |
| `uint32` | Number of patterns |

| Size | Detail |
|:---|:---|
| n_patterns * n_pixels * pixel_size | Patterns pixels (see "raw mode") |
| n_cells * `uint32` | Patterns indexes (0-based) as `uint32` |
