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

#ifdef TARGET_EXTENSION
#define RIF_PLAYDATE
#endif

#ifdef RIF_PLAYDATE
#include "pd_api.h"
#endif

#ifdef RIF_PLAYDATE
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
    
    int width;
    int height;
    
    int x;
    int y;
    
    float rotation;
    
    float centerX_multiplier;
    float centerY_multiplier;
    
    int size_width;
    int size_height;
} RIF_OpaqueImage;

typedef struct {
    RIF_OpaqueImage *opaque;
    RIF_Pool *pool;
    
    bool hasAlpha;
    
    int width;
    int height;
    
    size_t totalBytes;
    size_t readBytes;
    
    uint8_t *pixels;
    RIF_Pixel *pixels_a;

	#ifdef RIF_PLAYDATE
    SDFile *pd_file;
	#else
    FILE *file;
	#endif
} RIF_Image;

typedef struct {
    RIF_OpaqueImage *opaque;
    RIF_Pool *pool;
    
    bool hasAlpha;
    
    int width;
    int height;
    
    unsigned int patternSize;
    
    unsigned int numberOfPatterns;
    unsigned int cellCols;
    unsigned int cellRows;
    
    unsigned int numberOfCells;

    int patternsRead;
    int cellsRead;

    size_t readBytes;
    size_t totalBytes;
    
    RIF_Cell *cells;
	RIF_Cell_A *cells_a;

	RIF_Pattern **patterns;
	RIF_Pattern_A **patterns_a;

	#ifdef RIF_PLAYDATE
    SDFile *pd_file;
	#else
    FILE *file;
	#endif
} RIF_CImage;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} RIF_Rect;

RIF_Pool* librif_pool_new(size_t size);
void librif_pool_clear(RIF_Pool *pool);
void librif_pool_free(RIF_Pool *pool);

RIF_Image* librif_image_open(const char *filename, RIF_Pool *pool);
bool librif_image_read(RIF_Image *image, size_t size, bool *closed);

void librif_image_get_pixel(RIF_Image *image, int x, int y, uint8_t *color, uint8_t *alpha);
void librif_image_set_pixel(RIF_Image *image, int x, int y, uint8_t color, uint8_t alpha);

RIF_Image* librif_image_copy(RIF_Image *source);
RIF_Image* librif_image_transform(RIF_OpaqueImage *source);

void librif_image_free(RIF_Image *image);

RIF_CImage* librif_cimage_open(const char *filename, RIF_Pool *pool);
bool librif_cimage_read(RIF_CImage *image, size_t size, bool *closed);
void librif_cimage_get_pixel(RIF_CImage *image, int x, int y, uint8_t *color, uint8_t *alpha);
RIF_Image* librif_cimage_decompress(RIF_CImage *cimage, RIF_Pool *pool);
void librif_cimage_free(RIF_CImage *image);

void librif_opaque_set_position(RIF_OpaqueImage *image, int x, int y);
void librif_opaque_set_rotation(RIF_OpaqueImage *image, float angle);
void librif_opaque_set_size(RIF_OpaqueImage *image, int width, int height);
void librif_opaque_set_center(RIF_OpaqueImage *image, float x_multiplier, float y_multiplier);

void librif_opaque_reset_transform(RIF_OpaqueImage *image);

#ifdef RIF_PLAYDATE
LCDBitmap* librif_opaque_image_to_bitmap(RIF_OpaqueImage *image);
#endif

// Graphics

typedef enum {
    RIF_DitherTypeBayer2,
    RIF_DitherTypeBayer4,
    RIF_DitherTypeBayer8
} RIF_DitherType;

void librif_gfx_init(void);

void librif_gfx_set_dither_type(RIF_DitherType type);

void librif_gfx_set_blend_color(uint8_t color);
void librif_gfx_clear_blend_color(void);

RIF_Rect librif_gfx_get_draw_bounds(void);

void librif_gfx_draw_image(RIF_OpaqueImage *image);
void librif_gfx_draw_image_into(RIF_OpaqueImage *image, RIF_Image *dstImage);

#endif /* librif_h */
