//
//  librif.h
//  librif
//
//  Created by Matteo D'Ignazio on 16/08/21.
//

#ifndef librif_h
#define librif_h

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef PLAYDATE
#include "pd_api.h"
#endif

#ifdef PLAYDATE
extern PlaydateAPI *RIF_pd;
#endif

typedef struct {
    uint8_t color;
    uint8_t alpha;
} RIF_Pixel;

typedef struct {
	uint8_t *pixels;
} RIF_Pattern;

typedef struct {
    RIF_Pixel *pixels;
} RIF_Pattern_A;

typedef struct {
    RIF_Pattern *pattern;
} RIF_Cell;

typedef struct {
    RIF_Pattern_A *pattern;
} RIF_Cell_A;

typedef struct {
    size_t size;
    void *address;
    void *startAddress;
} RIF_Pool;

typedef struct {
    void *image;
    
    bool compressed;
    
    bool hasAlpha;
    
    uint32_t width;
    uint32_t height;
} RIF_OpaqueImage;

typedef struct {
    RIF_OpaqueImage *opaque;
    RIF_Pool *pool;
    
    bool hasAlpha;
    
    uint32_t width;
    uint32_t height;
    
    size_t totalBytes;
    size_t readBytes;
    
    uint8_t *pixels;
    RIF_Pixel *pixels_a;

	#ifdef PLAYDATE
    SDFile *pd_file;
	#else
    FILE *stream;
	#endif
} RIF_Image;

typedef struct {
    RIF_OpaqueImage *opaque;
    RIF_Pool *pool;
    
    bool hasAlpha;
    
    uint32_t width;
    uint32_t height;
    
    uint32_t patternSize;
    uint32_t numberOfPatterns;
    uint32_t cellCols;
    uint32_t cellRows;
    
    unsigned int numberOfCells;

    uint32_t patternsRead;
    uint32_t cellsRead;

    size_t readBytes;
    size_t totalBytes;
    
    RIF_Cell *cells;
	RIF_Cell_A *cells_a;

	RIF_Pattern **patterns;
	RIF_Pattern_A **patterns_a;

	#ifdef PLAYDATE
    SDFile *pd_file;
	#else
    FILE *stream;
	#endif
} RIF_CImage;

RIF_Pool* librif_pool_new(size_t size);
void librif_pool_clear(RIF_Pool *pool);
void librif_pool_free(RIF_Pool *pool);

RIF_Image* librif_image_open(const char *filename, RIF_Pool *pool);
bool librif_image_read(RIF_Image *image, size_t size, bool *closed);
void librif_image_get_pixel(RIF_Image *image, uint32_t x, uint32_t y, uint8_t *color, uint8_t *alpha);
void librif_image_free(RIF_Image *image);

RIF_CImage* librif_cimage_open(const char *filename, RIF_Pool *pool);
bool librif_cimage_read(RIF_CImage *image, size_t size, bool *closed);
void librif_cimage_get_pixel(RIF_CImage *image, uint32_t x, uint32_t y, uint8_t *color, uint8_t *alpha);
RIF_Image* librif_cimage_decompress(RIF_CImage *cimage, RIF_Pool *pool);
void librif_cimage_free(RIF_CImage *image);

// Graphics

typedef enum {
    RIF_DitherTypeBayer2,
    RIF_DitherTypeBayer4,
    RIF_DitherTypeBayer8
} RIF_DitherType;

void librif_gfx_init(void);

void librif_gfx_set_dither_type(RIF_DitherType type);

void librif_gfx_draw_image(RIF_OpaqueImage *image, int x, int y);
void librif_gfx_draw_scaled_image(RIF_OpaqueImage *image, int x, int y, unsigned int width, unsigned int height);

#endif /* librif_h */
