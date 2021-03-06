//
//  librif.c
//  librif
//
//  Created by Matteo D'Ignazio on 16/08/21.
//

#include "librif.h"

#ifdef RIF_PLAYDATE
PlaydateAPI *RIF_pd;
#endif

#define RIF_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define RIF_MIN(x, y) (((x) < (y)) ? (x) : (y))

#ifdef RIF_PLAYDATE

#define RIF_LCD_ROWS LCD_ROWS
#define RIF_LCD_COLUMNS LCD_COLUMNS

#else

#define RIF_LCD_ROWS 100
#define RIF_LCD_COLUMNS 100

#endif

typedef struct {
    int x;
    int y;
} RIF_Point;

typedef enum {
    RIF_GFX_ContextTypeLCD,
    RIF_GFX_ContextTypeBitmap,
    RIF_GFX_ContextTypeImage
} RIF_GFX_ContextType;

typedef struct {
    RIF_GFX_ContextType type;
    RIF_Image *dstImage;
    #ifdef RIF_PLAYDATE
    uint8_t *pd_framebuffer;
    #endif
    int cols;
    int rows;
} RIF_GFX_Context;

typedef struct {
    bool rotated;
    bool x_scaled, y_scaled;
    float scaleX, scaleY;
    int width, height;
    int x, y;
    float centerX, centerY;
    float rotation;
    float centerX_multiplier, centerY_multiplier;
} RIF_GFX_Transform;

typedef struct {
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    bool min_set;
} RIF_DrawBounds;

static bool gfx_init_flag = false;
static RIF_DitherType gfx_dither_type = RIF_DitherTypeBayer4;

static uint8_t gfx_blend_color = 0;
static bool gfx_has_blend_color = false;

static RIF_DrawBounds gfx_draw_bounds;

static const size_t patternIndexInBytes = 4;

typedef uint8_t(*RIF_GFX_ditherFunction)(uint8_t col, uint8_t row);

static const uint8_t RIF_bayer2[2][2] = {
    {     51, 206    },
    {    153, 102    }
};

static const uint8_t RIF_bayer4[4][4] = {
    {     15, 195,  60, 240    },
    {    135,  75, 180, 120    },
    {     45, 225,  30, 210    },
    {    165, 105, 150,  90    }
};

static const uint8_t RIF_bayer8[8][8] = {
    {      0, 128,  32, 160,   8, 136,  40, 168    },
    {    192,  64, 224,  96, 200,  72, 232, 104    },
    {     48, 176,  16, 144,  56, 184,  24, 152    },
    {    240, 112, 208,  80, 248, 120, 216,  88    },
    {     12, 140,  44, 172,   4, 132,  36, 164    },
    {    204,  76, 236, 108, 196,  68, 228, 100    },
    {     60, 188,  28, 156,  52, 180,  20, 148    },
    {    252, 124, 220,  92, 244, 116, 212,  84    }
};

static uint8_t RIF_bayer2_function(uint8_t col, uint8_t row){
    return RIF_bayer2[col][row];
}

static uint8_t RIF_bayer4_function(uint8_t col, uint8_t row){
    return RIF_bayer4[col][row];
}

static uint8_t RIF_bayer8_function(uint8_t col, uint8_t row){
    return RIF_bayer8[col][row];
}

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

static inline void librif_opaque_get_pixel(RIF_OpaqueImage *image, int x, int y, uint8_t *color, uint8_t *alpha);

static RIF_OpaqueImage* librif_opaque_from_image(RIF_Image *image);
static RIF_OpaqueImage* librif_opaque_from_cimage(RIF_CImage *image);
static void librif_opaque_free(RIF_OpaqueImage *image);

static void* librif_malloc(size_t size);
static void librif_free(void *ptr);

static void librif_gfx_init(void);

static void librif_gfx_begin_draw(RIF_GFX_Context *context);
static void librif_gfx_end_draw(RIF_GFX_Context *context);

static RIF_GFX_Context librif_gfx_context_new(RIF_GFX_ContextType type);
static void librif_gfx_draw_image_context(RIF_OpaqueImage *image, RIF_GFX_Context *context);

static void librif_gfx_will_draw_pixel(RIF_OpaqueImage *opaqueImage, int x, int y);
static void librif_gfx_draw_pixel(RIF_OpaqueImage *opaqueImage, RIF_GFX_Context *context, uint8_t color, uint8_t alpha, int x, int y, int d_col, int d_row, int fb_index, RIF_GFX_ditherFunction ditherFunction);

static RIF_Rect librif_gfx_get_transform_rect(RIF_GFX_Transform *transform);
static RIF_GFX_Transform librif_get_transform(RIF_OpaqueImage *image);

static void librif_gfx_draw_image_to_bitmap(RIF_OpaqueImage *image, int width, int height);

static RIF_DrawBounds RIF_DrawBounds_new(void);

RIF_Image* librif_image_base(void){

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
    
    image->opaque = librif_opaque_from_image(image);
    
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
    
    image->opaque = librif_opaque_from_image(image);
    
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
    
    copied->opaque = librif_opaque_from_image(copied);
    
    return copied;
}

RIF_Image* librif_image_transform(RIF_OpaqueImage *source){
    
    RIF_GFX_Transform transform = librif_get_transform(source);
    RIF_Rect rect = librif_gfx_get_transform_rect(&transform);
    
    // save position
    int x = source->x;
    int y = source->y;
    
    librif_opaque_set_position(source, -rect.x, -rect.y);
    
    RIF_Image *image = librif_image_new(rect.width, rect.height);
    
    librif_gfx_draw_image_into(source, image);
    
    // restore position
    librif_opaque_set_position(source, x, y);
    
    return image;
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
        image->cells = pool->address;
        pool->address += cellsSizeInBytes;
        
        image->patterns = pool->address;
        pool->address += patternsSizeInBytes;
    }
    else {
        image->cells = librif_malloc(cellsSizeInBytes);
        image->patterns = librif_malloc(patternsSizeInBytes);
    }

    image->opaque = librif_opaque_from_cimage(image);
        
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

void librif_cimage_read_patterns(RIF_CImage *image, size_t size){
        
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

void librif_cimage_read_cells(RIF_CImage *image, size_t size){

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
    
    image->opaque = librif_opaque_from_image(image);
    
    return image;
}

RIF_OpaqueImage* librif_opaque_new(void){
    RIF_OpaqueImage *opaqueImage = librif_malloc(sizeof(RIF_OpaqueImage));
    
    librif_opaque_reset_transform(opaqueImage);
    
    return opaqueImage;
}

void librif_opaque_reset_transform(RIF_OpaqueImage *image){
    image->rotation = 0;
    
    image->size_width = -1;
    image->size_height = -1;
    
    image->x = 0;
    image->y = 0;

    image->centerX_multiplier = 0;
    image->centerY_multiplier = 0;
}

RIF_OpaqueImage* librif_opaque_from_image(RIF_Image *image){
    RIF_OpaqueImage *opaqueImage = librif_opaque_new();
    
    opaqueImage->compressed = false;
    opaqueImage->image = image;
    opaqueImage->width = image->width;
    opaqueImage->height = image->height;
    opaqueImage->hasAlpha = image->hasAlpha;
    
    return opaqueImage;
}

RIF_OpaqueImage* librif_opaque_from_cimage(RIF_CImage *image){
    RIF_OpaqueImage *opaqueImage = librif_opaque_new();
    
    opaqueImage->compressed = true;
    opaqueImage->image = image;
    opaqueImage->width = image->width;
    opaqueImage->height = image->height;
    opaqueImage->hasAlpha = image->hasAlpha;
    
    return opaqueImage;
}

void librif_opaque_set_position(RIF_OpaqueImage *image, int x, int y){
    image->x = x;
    image->y = y;
}

void librif_opaque_set_rotation(RIF_OpaqueImage *image, float angle){
    image->rotation = angle;
}

void librif_opaque_set_size(RIF_OpaqueImage *image, int width, int height){
    image->size_width = width;
    image->size_height = height;
}

void librif_opaque_set_center(RIF_OpaqueImage *image, float x_multiplier, float y_multiplier){
    
    image->centerX_multiplier = x_multiplier;
    image->centerY_multiplier = y_multiplier;
}

void librif_opaque_free(RIF_OpaqueImage *image){
    
    librif_free(image);
}

size_t get_pixels_size_in_bytes(int width, int height, bool alpha){
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

uint8_t librif_read_uint8(RIF_Image *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_1_buffer, 1);
    #else
    fread(rif_byte_1_buffer, 1, 1, image->file);
    #endif
    return rif_byte_1_buffer[0];
}

uint32_t librif_read_uint32(RIF_Image *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_4_buffer, 4);
    #else
    fread(rif_byte_4_buffer, 4, 1, image->file);
    #endif
    return rif_byte_4_buffer[0] << 24 | rif_byte_4_buffer[1] << 16 | rif_byte_4_buffer[2] << 8 | rif_byte_4_buffer[3];
}

uint8_t librifc_read_uint8(RIF_CImage *image){
    #ifdef RIF_PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_byte_1_buffer, 1);
    #else
    fread(rif_byte_1_buffer, 1, 1, image->file);
    #endif
    return rif_byte_1_buffer[0];
}

uint32_t librifc_read_uint32(RIF_CImage *image){
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
    
    librif_opaque_free(image->opaque);
    librif_free(image);
}

void librif_cimage_free(RIF_CImage *image){
	
    if(image->pool == NULL){
        librif_free(image->patterns);
        librif_free(image->cells);
    }

    librif_opaque_free(image->opaque);
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

void librif_pool_clear(RIF_Pool *pool) {
    pool->address = pool->startAddress;
    memset(pool->startAddress, 0, pool->size);
}

void librif_pool_free(RIF_Pool *pool) {
    librif_pool_clear(pool);
    
    librif_free(pool->startAddress);
    librif_free(pool);
}

static inline void librif_opaque_get_pixel(RIF_OpaqueImage *image, int x, int y, uint8_t *color, uint8_t *alpha){
    if(image->compressed){
        librif_cimage_get_pixel((RIF_CImage*)image->image, x, y, color, alpha);
    }
    else {
        librif_image_get_pixel((RIF_Image*)image->image, x, y, color, alpha);
    }
}

#ifdef RIF_PLAYDATE
LCDBitmap* librif_opaque_image_to_bitmap(RIF_OpaqueImage *image){
    
    RIF_GFX_Transform transform = librif_get_transform(image);
    RIF_Rect rect = librif_gfx_get_transform_rect(&transform);
    
    // save position
    int x = image->x;
    int y = image->y;
    
    librif_opaque_set_position(image, -rect.x, -rect.y);
    
    LCDBitmap *bitmap = RIF_pd->graphics->newBitmap(rect.width, rect.height, kColorClear);
    RIF_pd->graphics->pushContext(bitmap);
    
    librif_gfx_draw_image_to_bitmap(image, rect.width, rect.height);
    
    RIF_pd->graphics->popContext();
    
    // restore position
    librif_opaque_set_position(image, x, y);
    
    return bitmap;
}
#endif

// Drawing functions

void librif_gfx_init(void){
    
    if(!gfx_init_flag){
        gfx_init_flag = true;
        
        gfx_draw_bounds = RIF_DrawBounds_new();
    }
}

void librif_gfx_begin_draw(RIF_GFX_Context *context){
    gfx_draw_bounds = RIF_DrawBounds_new();
}

void librif_gfx_end_draw(RIF_GFX_Context *context){
    
    if(context->type == RIF_GFX_ContextTypeLCD){
        #ifdef RIF_PLAYDATE
        RIF_pd->graphics->markUpdatedRows(gfx_draw_bounds.min_y, gfx_draw_bounds.max_y);
        #endif
    }
}

RIF_Rect librif_gfx_get_draw_bounds(void){
    return (RIF_Rect){
        .x = gfx_draw_bounds.min_x,
        .y = gfx_draw_bounds.min_y,
        .width = abs(gfx_draw_bounds.max_x + 1 - gfx_draw_bounds.min_x),
        .height = abs(gfx_draw_bounds.max_y + 1 - gfx_draw_bounds.min_y),
    };
}

void librif_gfx_set_dither_type(RIF_DitherType type){
    gfx_dither_type = type;
}

void librif_gfx_set_blend_color(uint8_t color){
    gfx_has_blend_color = true;
    gfx_blend_color = color;
}

void librif_gfx_clear_blend_color(void){
    gfx_has_blend_color = false;
    gfx_blend_color = 0;
}

RIF_GFX_Context librif_gfx_context_new(RIF_GFX_ContextType type) {
    RIF_GFX_Context context = {
        .type = type,
        .dstImage = NULL,
        .cols = 0,
        .rows = 0
    };
    
    #ifdef RIF_PLAYDATE
    context.pd_framebuffer = NULL;
    #endif
    
    return context;
}

RIF_GFX_Transform librif_get_transform(RIF_OpaqueImage *image){
    
    bool rotated = (image->rotation != 0);
    
    bool x_scaled = false;
    bool y_scaled = false;

    float scaleX = 1;
    float scaleY = 1;
    
    int width = image->width;
    int height = image->height;
    
    if(image->size_width > 0 && image->width != image->size_width){
        width = image->size_width;
        x_scaled = true;
        scaleX = (float)image->width / image->size_width;
    }
    
    if(image->size_height > 0 && image->height != image->size_height){
        height = image->size_height;
        y_scaled = true;
        scaleY = (float)image->height / image->size_height;
    }
    
    float centerX = width * image->centerX_multiplier;
    float centerY = height * image->centerY_multiplier;
    
    return (RIF_GFX_Transform){
        .rotated = rotated,
        .width = width,
        .height = height,
        .x = image->x,
        .y = image->y,
        .x_scaled = x_scaled,
        .y_scaled = y_scaled,
        .scaleX = scaleX,
        .scaleY = scaleY,
        .centerX_multiplier = image->centerX_multiplier,
        .centerY_multiplier = image->centerY_multiplier,
        .centerX = centerX,
        .centerY = centerY,
        .rotation = image->rotation
    };
}

void librif_gfx_draw_image(RIF_OpaqueImage *image){
    
    RIF_GFX_Context context = librif_gfx_context_new(RIF_GFX_ContextTypeLCD);
    #ifdef RIF_PLAYDATE
    context.pd_framebuffer = RIF_pd->graphics->getFrame();
    #endif
    
    context.cols = RIF_LCD_COLUMNS;
    context.rows = RIF_LCD_ROWS;

    librif_gfx_draw_image_context(image, &context);
}

void librif_gfx_draw_image_into(RIF_OpaqueImage *image, RIF_Image *dstImage){
    
    RIF_GFX_Context context = librif_gfx_context_new(RIF_GFX_ContextTypeImage);
    context.dstImage = dstImage;
    
    context.cols = dstImage->width;
    context.rows = dstImage->height;

    librif_gfx_draw_image_context(image, &context);
}

void librif_gfx_draw_image_to_bitmap(RIF_OpaqueImage *image, int width, int height){
    
    RIF_GFX_Context context = librif_gfx_context_new(RIF_GFX_ContextTypeBitmap);
    
    context.cols = width;
    context.rows = height;

    librif_gfx_draw_image_context(image, &context);
}

void librif_gfx_draw_image_context(RIF_OpaqueImage *image, RIF_GFX_Context *context){
    librif_gfx_init();
    
    int d_mod = 4 - 1;
    
    if(gfx_dither_type == RIF_DitherTypeBayer2){
        d_mod = 2 - 1;
    }
    else if(gfx_dither_type == RIF_DitherTypeBayer8){
        d_mod = 8 - 1;
    }
    
    RIF_GFX_ditherFunction ditherFunction = RIF_bayer4_function;
    
    if(gfx_dither_type == RIF_DitherTypeBayer2){
        ditherFunction = RIF_bayer2_function;
    }
    else if(gfx_dither_type == RIF_DitherTypeBayer8){
        ditherFunction = RIF_bayer8_function;
    }
    
    RIF_GFX_Transform transform = librif_get_transform(image);
    RIF_Rect rect = librif_gfx_get_transform_rect(&transform);

    int offsetX = 0;
    int absoluteStartX = rect.x;
    int startX = absoluteStartX;
    
    if(absoluteStartX < 0){
        startX = 0;
        offsetX = - absoluteStartX;
    }
    
    int offsetY = 0;
    int absoluteStartY = rect.y;
    int startY = absoluteStartY;
    
    if(absoluteStartY < 0){
        startY = 0;
        offsetY = - absoluteStartY;
    }
    
    int endX = RIF_MIN(absoluteStartX + offsetX + rect.width, context->cols);
    int endY = RIF_MIN(absoluteStartY + offsetY + rect.height, context->rows);
    
    int cols = endX - startX;
    int rows = endY - startY;
    
    librif_gfx_begin_draw(context);
    
    if(transform.rotated){

        float rotation_sin = sinf( - transform.rotation * ((float)M_PI / 180.0f));
        float rotation_cos = cosf( - transform.rotation * ((float)M_PI / 180.0f));
        
        float d_x = (image->x - transform.centerX) - rect.x - offsetX;
        float d_y = (image->y - transform.centerY) - rect.y - offsetY;
        
        #ifdef RIF_PLAYDATE
        int lcd_rows = startY * LCD_ROWSIZE + startX / 8;
        #endif
        
        for(int y1 = 0; y1 < rows; y1++){
            int absoluteY = startY + y1;
            
            int d_row = absoluteY & d_mod;
            
            float y_c = y1 - d_y - transform.centerY;
            
            float y_sin = y_c * rotation_sin;
            float y_cos = y_c * rotation_cos;
            
            #ifdef RIF_PLAYDATE
            int fb_index = lcd_rows;
            #else
            int fb_index = 0;
            #endif
            
            for(int x1 = 0; x1 < cols; x1++){
                int absoluteX = startX + x1;
                
                #ifdef RIF_PLAYDATE
                if(x1 > 0 && (absoluteX & 7) == 0){
                    fb_index += 1;
                }
                #endif
                
                int d_col = absoluteX & d_mod;
                
                float x_c = x1 - d_x - transform.centerX;
                
                int imageX = roundf((x_c * rotation_cos - y_sin + transform.centerX) * transform.scaleX);
                int imageY = roundf((x_c * rotation_sin + y_cos + transform.centerY) * transform.scaleY);
                
                if(imageX >= 0 && imageX < image->width && imageY >= 0 && imageY < image->height){
                    uint8_t color, alpha;
                    librif_opaque_get_pixel(image, imageX, imageY, &color, &alpha);
                    
                    librif_gfx_draw_pixel(image, context, color, alpha, absoluteX, absoluteY, d_col, d_row, fb_index, ditherFunction);
                }
            }
            
            #ifdef RIF_PLAYDATE
            lcd_rows += LCD_ROWSIZE;
            #endif
        }
    }
    else {
        
        #ifdef RIF_PLAYDATE
        int lcd_rows = startY * LCD_ROWSIZE + startX / 8;
        #endif
        
        for(int y1 = 0; y1 < rows; y1++){
            int absoluteY = startY + y1;
            
            int d_row = absoluteY & d_mod;
            
            int imageY = offsetY + y1;
            if(transform.y_scaled){
                imageY = roundf(imageY * transform.scaleY);
            }
            
            #ifdef RIF_PLAYDATE
            int fb_index = lcd_rows;
            #else
            int fb_index = 0;
            #endif
            
            for(int x1 = 0; x1 < cols; x1++){
                int absoluteX = startX + x1;
                
                #ifdef RIF_PLAYDATE
                if(x1 > 0 && (absoluteX & 7) == 0){
                    fb_index += 1;
                }
                #endif
                
                int d_col = absoluteX & d_mod;
                
                int imageX = offsetX + x1;
                if(transform.x_scaled){
                    imageX = roundf(imageX * transform.scaleX);
                }
                
                if(imageX >= 0 && imageX < image->width && imageY >= 0 && imageY < image->height){
                    uint8_t color, alpha;
                    librif_opaque_get_pixel(image, imageX, imageY, &color, &alpha);
                    
                    librif_gfx_draw_pixel(image, context, color, alpha, absoluteX, absoluteY, d_col, d_row, fb_index, ditherFunction);
                }
            }
            
            #ifdef RIF_PLAYDATE
            lcd_rows += LCD_ROWSIZE;
            #endif
        }
    }
    
    librif_gfx_end_draw(context);
}

void librif_gfx_will_draw_pixel(RIF_OpaqueImage *opaqueImage, int x, int y){
    
    if(!gfx_draw_bounds.min_set){
        gfx_draw_bounds.min_set = true;
        
        gfx_draw_bounds.min_x = x;
        gfx_draw_bounds.max_x = x;
        gfx_draw_bounds.min_y = y;
        gfx_draw_bounds.max_y = y;
    }
    
    gfx_draw_bounds.min_x = RIF_MIN(gfx_draw_bounds.min_x, x);
    gfx_draw_bounds.max_x = RIF_MAX(gfx_draw_bounds.max_x, x);
    
    gfx_draw_bounds.min_y = RIF_MIN(gfx_draw_bounds.min_y, y);
    gfx_draw_bounds.max_y = RIF_MAX(gfx_draw_bounds.max_y, y);
}

static void librif_gfx_draw_pixel(RIF_OpaqueImage *opaqueImage, RIF_GFX_Context *context, uint8_t color, uint8_t alpha, int x, int y, int d_col, int d_row, int fb_index, RIF_GFX_ditherFunction ditherFunction){
    
    if(alpha == 0){
        return;
    }
    
    bool drawPixel = true;
    
    if(gfx_has_blend_color && color == gfx_blend_color){
        drawPixel = false;
    }
    
    if(drawPixel){
        
        if(context->type == RIF_GFX_ContextTypeLCD || context->type == RIF_GFX_ContextTypeBitmap){
            
            bool drawPixel2 = true;
            
            if(opaqueImage->hasAlpha){
                if(alpha < 128){
                    drawPixel2 = false;
                }
            }
            
            if(drawPixel2){
                librif_gfx_will_draw_pixel(opaqueImage, x, y);
                
                uint8_t d_color = ditherFunction(d_col, d_row);
                
                uint8_t adjustedColor = color;
                
                if(opaqueImage->hasAlpha){
                    adjustedColor = color * alpha / 255.0f;
                }
                
                if(context->type == RIF_GFX_ContextTypeLCD){
                    #ifdef RIF_PLAYDATE
                    int mask = (1 << (7 - (x & 7)));
                    context->pd_framebuffer[fb_index] ^= (-(adjustedColor >= d_color) ^ context->pd_framebuffer[fb_index]) & mask;
                    #endif
                }
                else if(context->type == RIF_GFX_ContextTypeBitmap){
                    
                    #ifdef RIF_PLAYDATE
                    RIF_pd->graphics->fillRect(x, y, 1, 1, (adjustedColor < d_color) ? kColorBlack : kColorWhite);
                    #endif
                }
            }
        }
        else if(context->type == RIF_GFX_ContextTypeImage){
            librif_gfx_will_draw_pixel(opaqueImage, x, y);
            
            RIF_Image *dstImage = context->dstImage;
            
            if(dstImage->hasAlpha){
                size_t i = (y * context->cols + x) * 2;
                
                uint8_t *pixel_color = &dstImage->pixels[i];
                uint8_t *pixel_alpha = &dstImage->pixels[i + 1];

                uint8_t result_color = color;
                uint8_t result_alpha = alpha;
                
                if(alpha < 255){
                    int alpha_inverse = 255 - (int)alpha;
                    result_color = (alpha_inverse * (*pixel_color) + alpha * color) / 255.0f;
                    result_alpha = 255 - ((255 - (*pixel_alpha)) * alpha_inverse / 255.0f);
                }
                
                *pixel_color = result_color;
                *pixel_alpha = result_alpha;
            }
            else {
                size_t i = y * context->cols + x;
                
                uint8_t result_color = color;
                
                if(alpha < 255){
                    result_color = ((255 - (int)alpha) * dstImage->pixels[i] + alpha * color) / 255.0f;
                }
                
                dstImage->pixels[i] = result_color;
            }
        }
    }
}

RIF_Rect librif_gfx_get_transform_rect(RIF_GFX_Transform *transform){
        
    unsigned int n = 4;
    RIF_Point points[n];
    
    points[0] = (RIF_Point){
        .x = 0,
        .y = 0
    };
    points[1] = (RIF_Point){
        .x = transform->width,
        .y = 0
    };
    points[2] = (RIF_Point){
        .x = transform->width,
        .y = (transform->height - 1)
    };
    points[3] = (RIF_Point){
        .x = 0,
        .y = (transform->height - 1)
    };
    
    RIF_DrawBounds bounds = RIF_DrawBounds_new();
    
    float rotation_sin = sinf(transform->rotation * ((float)M_PI / 180));
    float rotation_cos = cosf(transform->rotation * ((float)M_PI / 180));
    
    for(int i = 0; i < n; i++){
        
        int x = points[i].x;
        int y = points[i].y;
        
        if(transform->rotated){
            x = x - transform->centerX;
            y = y - transform->centerY;
            
            int rotated_x = ceilf(x * rotation_cos - y * rotation_sin + transform->centerX);
            int rotated_y = ceilf(x * rotation_sin + y * rotation_cos + transform->centerY);
            
            x = rotated_x;
            y = rotated_y;
        }
        
        x += transform->x - transform->centerX;
        y += transform->y - transform->centerY;
        
        if(!bounds.min_set){
            bounds.min_set = true;
            
            bounds.min_x = x;
            bounds.max_x = x;
            bounds.min_y = y;
            bounds.max_y = y;
        }
        
        bounds.min_x = fminf(bounds.min_x, x);
        bounds.max_x = fmaxf(bounds.max_x, x);
        
        bounds.min_y = fminf(bounds.min_y, y);
        bounds.max_y = fmaxf(bounds.max_y, y);
    }
    
    return (RIF_Rect){
        .x = bounds.min_x,
        .y = bounds.min_y,
        .width = abs(bounds.max_x - bounds.min_x),
        .height = abs(bounds.max_y - bounds.min_y)
    };
}

static RIF_DrawBounds RIF_DrawBounds_new(void){
    return (RIF_DrawBounds){
        .min_x = 0,
        .max_x = 0,
        .min_y = 0,
        .max_y = 0,
        .min_set = false
    };
}

#ifdef RIF_PLAYDATE

void* librif_malloc(size_t size) {
    return RIF_pd->system->realloc(NULL, size);
}

void librif_free(void *ptr) {
    RIF_pd->system->realloc(ptr, 0);
}

#else

void* librif_malloc(size_t size) {
    return malloc(size);
}

void librif_free(void *ptr) {
    free(ptr);
}

#endif
