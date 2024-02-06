//
//  librif.h
//  librif
//
//  Created by Matteo D'Ignazio on 16/08/21.
//

#ifndef librif_h
#define librif_h

#ifdef TARGET_EXTENSION
#define RIF_PLAYDATE
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifdef RIF_PLAYDATE
#include "pd_api.h"
#endif

#ifdef RIF_PLAYDATE
extern PlaydateAPI *RIF_pd;
#endif

typedef struct {
    uint8_t *address;
    uint8_t *startAddress;
    size_t size;
} RIF_Pool;

typedef struct {
    uint8_t *pixels;
    
    bool hasAlpha;
    
    int width;
    int height;
    
	#ifdef RIF_PLAYDATE
    SDFile *pd_file;
	#else
    FILE *file;
	#endif
    
    size_t totalBytes;
    size_t readBytes;
    
    RIF_Pool *pool;
} RIF_Image;

typedef struct {
    uint8_t **cells;
    uint8_t *patterns;
    
    bool hasAlpha;
    
    int width;
    int height;
    
    unsigned int patternSize;
    unsigned int numberOfPatterns;
    unsigned int cellCols;
    unsigned int cellRows;
    unsigned int numberOfCells;
    
	#ifdef RIF_PLAYDATE
    SDFile *pd_file;
	#else
    FILE *file;
	#endif
    
    int cellsRead;

    size_t patternsReadBytes;
    size_t patternsTotalBytes;
    
    size_t readBytes;
    size_t totalBytes;
    
    RIF_Pool *pool;
} RIF_CImage;

#ifdef RIF_PLAYDATE
void librif_init(PlaydateAPI *pd);
#else
void librif_init(void);
#endif

RIF_Pool* librif_pool_new(size_t size);
void librif_pool_realloc(RIF_Pool *pool, size_t size);
void librif_pool_clear(RIF_Pool *pool);
void librif_pool_free(RIF_Pool *pool);

RIF_Image* librif_image_open(const char *filename, RIF_Pool *pool);
bool librif_image_read(RIF_Image *image, size_t size, bool *closed);

void librif_image_get_pixel(RIF_Image *image, int x, int y, uint8_t *color, uint8_t *alpha);
void librif_image_set_pixel(RIF_Image *image, int x, int y, uint8_t color, uint8_t alpha);

RIF_Image* librif_image_copy(RIF_Image *source);

void librif_image_free(RIF_Image *image);

RIF_CImage* librif_cimage_open(const char *filename, RIF_Pool *pool);
bool librif_cimage_read(RIF_CImage *image, size_t size, bool *closed);
void librif_cimage_get_pixel(RIF_CImage *image, int x, int y, uint8_t *color, uint8_t *alpha);

RIF_Image* librif_cimage_decompress(RIF_CImage *cimage, RIF_Pool *pool);
void librif_cimage_free(RIF_CImage *image);

#endif /* librif_h */
