//
//  librif.c
//  librif
//
//  Created by Matteo D'Ignazio on 16/08/21.
//

#include "librif.h"
#include <math.h>

#ifdef RIF_PLAYDATE
PlaydateAPI *RIF_pd;
#endif

static const size_t patternIndexInBytes = 4;

static uint8_t librif_read_uint8(RIF_Image *image);
static uint32_t librif_read_uint32(RIF_Image *image);

static uint8_t librifc_read_uint8(RIF_CImage *image);
static uint32_t librifc_read_uint32(RIF_CImage *image);

static uint8_t rif_byte_1_buffer[1];
static uint8_t rif_byte_4_buffer[4];

static RIF_Image* librif_image_base(void);

static size_t get_pixels_size_in_bytes(int width, int height, bool alpha);

static void librif_cimage_read_patterns(RIF_CImage *image, size_t size);
static void librif_cimage_read_cells(RIF_CImage *image, size_t size);

static void* librif_malloc(size_t size);
static void* librif_realloc(void *ptr, size_t size);
static void librif_free(void *ptr);

void librif_init_base(void){
    
}

#ifdef RIF_PLAYDATE
void librif_init(PlaydateAPI *pd){
    RIF_pd = pd;
    librif_init_base();
}
#else
void librif_init(void){
    librif_init_base();
}
#endif

static RIF_Image* librif_image_base(void){

    RIF_Image *image = librif_malloc(sizeof(RIF_Image));
    
    image->pool = NULL;
    
    image->width = 0;
    image->height = 0;
    
    image->readBytes = 0;
    image->totalBytes = 0;
    
    image->hasAlpha = false;
    
    image->pixels = NULL;
    
    #ifdef RIF_PLAYDATE
    image->pd_file = NULL;
    #else
    image->file = NULL;
    #endif
    
    return image;
}

RIF_Image* librif_image_open(const char *filename, RIF_Pool *pool){
    
    #ifdef RIF_PLAYDATE
    SDFile *file = RIF_pd->file->open(filename, kFileRead);
    if(file == NULL){
        return NULL;
    }
    #else
    FILE *file = fopen(filename, "rb");
    if(file == NULL){
        return NULL;
    }
    #endif

    RIF_Image *image = librif_image_base();
    image->pool = pool;
    
    #ifdef RIF_PLAYDATE
    image->pd_file = file;
    #else
    image->file = file;
    #endif
    
    uint8_t alphaChannelInt = librif_read_uint8(image);
    image->hasAlpha = (alphaChannelInt == 1) ? true : false;

    image->width = librif_read_uint32(image);
    image->height = librif_read_uint32(image);
    
    size_t pixelsSizeInBytes = get_pixels_size_in_bytes(image->width, image->height, image->hasAlpha);
    
    image->readBytes = 0;
    image->totalBytes = pixelsSizeInBytes;

    if(pool != NULL){
        image->pixels = pool->address;
        pool->address += pixelsSizeInBytes;
    }
    else {
        image->pixels = librif_malloc(pixelsSizeInBytes);
    }
    
    return image;
}

bool librif_image_read(RIF_Image *image, size_t size, bool *closed){
    
    if(closed != NULL){
        *closed = false;
    }
    
    bool closeFile = false;
    
    size_t chunks = image->totalBytes;
    if(size > 0){
        chunks = size;
    }
    
    if((image->readBytes + chunks) >= image->totalBytes){
        chunks = image->totalBytes - image->readBytes;
        closeFile = true;
    }
    
    void *buffer = &image->pixels[image->readBytes];
    
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, buffer, (unsigned int)chunks);
    #else
    fread(buffer, 1, chunks, image->file);
    #endif
    
    image->readBytes += chunks;

    if(closeFile){
        if(closed != NULL){
            *closed = true;
        }
                
        #ifdef RIF_PLAYDATE
        RIF_pd->file->close(image->pd_file);
        image->pd_file = NULL;
        #else
        fclose(image->file);
        image->file = NULL;
        #endif
    }
    
    return true;
}

void librif_image_get_pixel(RIF_Image *image, int x, int y, uint8_t *color, uint8_t *alpha){

    if(x < 0 || x >= image->width || y < 0 || y >= image->height){
        *color = 0;
        if(alpha != NULL){
            *alpha = 255;
        }
        return;
    }
    
    if(image->hasAlpha){
        size_t i = (y * image->width + x) * 2;
        
        *color = image->pixels[i];
        if(alpha != NULL){
            *alpha = image->pixels[i + 1];
        }
    }
    else {
        *color = image->pixels[y * image->width + x];
        if(alpha != NULL){
            *alpha = 255;
        }
    }
}

RIF_Image* librif_image_new(int width, int height){
    
    RIF_Image *image = librif_image_base();
    
    image->hasAlpha = true;
    
    image->width = width;
    image->height = height;
        
    size_t pixelsSizeInBytes = get_pixels_size_in_bytes(image->width, image->height, image->hasAlpha);
    
    image->pixels = librif_malloc(pixelsSizeInBytes);
    memset(&image->pixels, 0, pixelsSizeInBytes);
    
    return image;
}

RIF_Image* librif_image_copy(RIF_Image *image){
    
    RIF_Image *copied = librif_image_base();
    
    copied->hasAlpha = image->hasAlpha;
    
    copied->width = image->width;
    copied->height = image->height;
    
    size_t size = get_pixels_size_in_bytes(copied->width, copied->height, copied->hasAlpha);
    copied->pixels = librif_malloc(size);
    memcpy(copied->pixels, image->pixels, size);
    
    return copied;
}

void librif_image_set_pixel(RIF_Image *image, int x, int y, uint8_t color, uint8_t alpha){
    if(x >= 0 && x < image->width && y >= 0 && y < image->height){
        
        if(image->hasAlpha){
            size_t i = (y * image->width + x) * 2;
            
            image->pixels[i] = color;
            image->pixels[i + 1] = alpha;
        }
        else {
            image->pixels[y * image->width + x] = color;
        }
    }
}

RIF_CImage* librif_cimage_open(const char *filename, RIF_Pool *pool){
    
    #ifdef RIF_PLAYDATE
    SDFile *file = RIF_pd->file->open(filename, kFileRead);
    if(file == NULL){
        return NULL;
    }
    #else
    FILE *file = fopen(filename, "rb");
    if(file == NULL){
        return NULL;
    }
    #endif
    
    RIF_CImage *image = librif_malloc(sizeof(RIF_CImage));
    image->pool = pool;
    
    #ifdef RIF_PLAYDATE
    image->pd_file = file;
    #else
    image->file = file;
    #endif
    
    uint8_t alphaChannelInt = librifc_read_uint8(image);
    image->hasAlpha = (alphaChannelInt == 1) ? true : false;

    image->width = librifc_read_uint32(image);
    image->height = librifc_read_uint32(image);

    unsigned int cx = librifc_read_uint32(image);
    unsigned int cy = librifc_read_uint32(image);

    image->cellCols = cx;
    image->cellRows = cy;

    unsigned int patternSize = librifc_read_uint32(image);
    image->patternSize = patternSize;

    unsigned int numberOfCells = cx * cy;
    image->numberOfCells = numberOfCells;

    unsigned int numberOfPatterns = librifc_read_uint32(image);
    image->numberOfPatterns = numberOfPatterns;

    size_t pixelsSizeInBytes = get_pixels_size_in_bytes(image->patternSize, image->patternSize, image->hasAlpha);

    size_t cellsSizeInBytes = numberOfCells * sizeof(uint8_t*);
    size_t patternsSizeInBytes = numberOfPatterns * pixelsSizeInBytes;
    
    image->readBytes = 0;
    image->totalBytes = patternsSizeInBytes + numberOfCells * patternIndexInBytes;
    
    image->patternsReadBytes = 0;
    image->patternsTotalBytes = patternsSizeInBytes;
    
    image->cellsRead = 0;
    
    image->patterns = NULL;
    image->cells = NULL;
    
    if(pool != NULL){
        image->cells = (uint8_t**)pool->address;
        pool->address += cellsSizeInBytes;
        
        image->patterns = pool->address;
        pool->address += patternsSizeInBytes;
    }
    else {
        image->cells = librif_malloc(cellsSizeInBytes);
        image->patterns = librif_malloc(patternsSizeInBytes);
    }
        
    return image;
}

bool librif_cimage_read(RIF_CImage *image, size_t size, bool *closed){
    
    if(closed != NULL){
        *closed = false;
    }
    
    bool closeFile = false;
    
    if(size > 0){
        if(image->patternsReadBytes < image->patternsTotalBytes){
            librif_cimage_read_patterns(image, size);
        }
        else if(image->cellsRead < image->numberOfCells){
            librif_cimage_read_cells(image, size);
        }
        
        if(image->cellsRead >= image->numberOfCells){
            closeFile = true;
        }
    }
    else {
        librif_cimage_read_patterns(image, 0);
        librif_cimage_read_cells(image, 0);
        
        closeFile = true;
    }
    
    if(closeFile){
        if(closed != NULL){
            *closed = true;
        }
        
        #ifdef RIF_PLAYDATE
        RIF_pd->file->close(image->pd_file);
        image->pd_file = NULL;
        #else
        fclose(image->file);
        image->file = NULL;
        #endif
    }
    
    return true;
}

void librif_cimage_get_pixel(RIF_CImage *image, int x, int y, uint8_t *color, uint8_t *alpha) {

    if(x < 0 || x >= image->width || y < 0 || y >= image->height){
        *color = 0;
        if(alpha != NULL){
            *alpha = 255;
        }
        return;
    }

    int patternSize = image->patternSize;

    int cellCol = x / patternSize;
    int cellRow = y / patternSize;

    int patternX = x - cellCol * patternSize;
    int patternY = y - cellRow * patternSize;

    int cell_i = cellRow * image->cellCols + cellCol;
    uint8_t *pattern = image->cells[cell_i];
    
    if(image->hasAlpha){
        size_t pixel_i = (patternY * patternSize + patternX) * 2;
        *color = pattern[pixel_i];
        if(alpha != NULL){
            *alpha = pattern[pixel_i + 1];
        }
    }
    else {
        size_t pixel_i = patternY * patternSize + patternX;
        *color = pattern[pixel_i];
        if(alpha != NULL){
            *alpha = 255;
        }
    }
}

static void librif_cimage_read_patterns(RIF_CImage *image, size_t size){
        
    size_t chunks = image->patternsTotalBytes;
    if(size > 0){
        chunks = size;
    }
    
    if((image->patternsReadBytes + chunks) >= image->patternsTotalBytes){
        chunks = image->patternsTotalBytes - image->patternsReadBytes;
    }
    
    void *buffer = &image->patterns[image->patternsReadBytes];

    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, buffer, (unsigned int)chunks);
    #else
    fread(buffer, 1, chunks, image->file);
    #endif
    
    image->patternsReadBytes += chunks;
    image->readBytes += chunks;
}

static void librif_cimage_read_cells(RIF_CImage *image, size_t size){

    int chunks = image->numberOfCells;
    if(size > 0){
        chunks = fmaxf(1, (float)size / patternIndexInBytes);
    }
    
    if((image->cellsRead + chunks) >= image->numberOfCells){
        chunks = image->numberOfCells - image->cellsRead;
    }

    size_t bufferSize = chunks * patternIndexInBytes;
    void *buffer = librif_malloc(bufferSize);
    
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, buffer, (unsigned int)bufferSize);
    #else
    fread(buffer, patternIndexInBytes, chunks, image->file);
    #endif
    
    size_t pixelsSizeInBytes = get_pixels_size_in_bytes(image->patternSize, image->patternSize, image->hasAlpha);
    int endRead = image->cellsRead + chunks;
        
    uint8_t *bufferPtr = buffer;
    
    for(int i = image->cellsRead; i < endRead; i++){
        uint32_t patternIndex = bufferPtr[0] << 24 | bufferPtr[1] << 16 | bufferPtr[2] << 8 | bufferPtr[3];
        image->cells[i] = &image->patterns[patternIndex * pixelsSizeInBytes];
        bufferPtr += patternIndexInBytes;
    }
    
    librif_free(buffer);
    
    image->cellsRead += chunks;
    image->readBytes += bufferSize;
}

RIF_Image* librif_cimage_decompress(RIF_CImage *cimage, RIF_Pool *pool){
    
    RIF_Image *image = librif_image_base();
    image->pool = pool;
    
    image->readBytes = 0;
    image->totalBytes = 0;
    
    image->width = cimage->width;
    image->height = cimage->height;
    image->hasAlpha = cimage->hasAlpha;
    
    size_t pixelsSizeInBytes = get_pixels_size_in_bytes(image->width, image->height, image->hasAlpha);
    
    if(pool != NULL){
        image->pixels = pool->address;
        pool->address += pixelsSizeInBytes;
    }
    else {
        image->pixels = librif_malloc(pixelsSizeInBytes);
    }
    
    for(int y = 0; y < image->height; y++){
        for(int x = 0; x < image->width; x++){
            
            uint8_t color, alpha;
            librif_cimage_get_pixel(cimage, x, y, &color, &alpha);

            if(image->hasAlpha){
                size_t i = (y * image->width + x) * 2;
                image->pixels[i] = color;
                image->pixels[i + 1] = alpha;
            }
            else {
                size_t i = y * image->width + x;
                image->pixels[i] = color;
            }
        }
    }
    
    return image;
}

static size_t get_pixels_size_in_bytes(int width, int height, bool alpha){
    size_t numberOfPixels = width * height;
    size_t size;
    if(alpha){
        size = numberOfPixels * sizeof(uint8_t) * 2;
    }
    else {
        size = numberOfPixels * sizeof(uint8_t);
    }
    return size;
}

static uint8_t librif_read_uint8(RIF_Image *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_1_buffer, 1);
    #else
    fread(rif_byte_1_buffer, 1, 1, image->file);
    #endif
    return rif_byte_1_buffer[0];
}

static uint32_t librif_read_uint32(RIF_Image *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_4_buffer, 4);
    #else
    fread(rif_byte_4_buffer, 4, 1, image->file);
    #endif
    return rif_byte_4_buffer[0] << 24 | rif_byte_4_buffer[1] << 16 | rif_byte_4_buffer[2] << 8 | rif_byte_4_buffer[3];
}

static uint8_t librifc_read_uint8(RIF_CImage *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_1_buffer, 1);
    #else
    fread(rif_byte_1_buffer, 1, 1, image->file);
    #endif
    return rif_byte_1_buffer[0];
}

static uint32_t librifc_read_uint32(RIF_CImage *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_4_buffer, 4);
    #else
    fread(rif_byte_4_buffer, 4, 1, image->file);
    #endif
    return rif_byte_4_buffer[0] << 24 | rif_byte_4_buffer[1] << 16 | rif_byte_4_buffer[2] << 8 | rif_byte_4_buffer[3];
}

void librif_image_free(RIF_Image *image){
    
    if(image->pool == NULL){
        librif_free(image->pixels);
    }
    
    librif_free(image);
}

void librif_cimage_free(RIF_CImage *image){
	
    if(image->pool == NULL){
        librif_free(image->patterns);
        librif_free(image->cells);
    }
    
    librif_free(image);
}

RIF_Pool* librif_pool_new(size_t size){
    void *ptr = librif_malloc(size);
    
    RIF_Pool *pool = librif_malloc(sizeof(RIF_Pool));
    
    pool->size = size;
    pool->startAddress = ptr;
    pool->address = ptr;

    return pool;
}

void librif_pool_clear(RIF_Pool *pool){
    pool->address = pool->startAddress;
    memset(pool->startAddress, 0, pool->size);
}

void librif_pool_realloc(RIF_Pool *pool, size_t size){
    pool->size = size;
    pool->address = librif_realloc(pool->address, size);
}

void librif_pool_free(RIF_Pool *pool){
    librif_pool_clear(pool);
    
    librif_free(pool->startAddress);
    librif_free(pool);
}

#ifdef RIF_PLAYDATE

static void* librif_malloc(size_t size){
    return RIF_pd->system->realloc(NULL, size);
}

static void* librif_realloc(void *ptr, size_t size){
    return RIF_pd->system->realloc(ptr, size);
}

static void librif_free(void *ptr){
    RIF_pd->system->realloc(ptr, 0);
}

#else

static void* librif_malloc(size_t size){
    return malloc(size);
}

static void* librif_realloc(void *ptr, size_t size){
    return realloc(ptr, size);
}

static void librif_free(void *ptr){
    free(ptr);
}

#endif
