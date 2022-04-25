//
//  librif.c
//  librif
//
//  Created by Matteo D'Ignazio on 16/08/21.
//

#include "librif.h"

#ifdef PLAYDATE
PlaydateAPI *RIF_pd;
#endif

#define RIF_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define RIF_MIN(x, y) (((x) < (y)) ? (x) : (y))

#ifdef PLAYDATE

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
    #ifdef PLAYDATE
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

static uint8_t RIF_bayer2_rows[RIF_LCD_ROWS];
static uint8_t RIF_bayer2_cols[RIF_LCD_COLUMNS];

static uint8_t RIF_bayer4_rows[RIF_LCD_ROWS];
static uint8_t RIF_bayer4_cols[RIF_LCD_COLUMNS];

static uint8_t RIF_bayer8_rows[RIF_LCD_ROWS];
static uint8_t RIF_bayer8_cols[RIF_LCD_COLUMNS];

typedef uint8_t(*RIF_GFX_ditherFunction)(uint8_t col, uint8_t row);

#ifdef PLAYDATE

typedef struct {
    uint16_t i;
    uint8_t black_mask;
    uint8_t white_mask;
} RIF_PD_Pixel;

static RIF_PD_Pixel RIF_pd_pixels[LCD_ROWS * LCD_COLUMNS];

#endif

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

static uint8_t librif_byte_int1(RIF_Image *image);
static uint32_t librif_byte_int4(RIF_Image *image);

static uint8_t librifc_byte_int1(RIF_CImage *image);
static uint32_t librifc_byte_int4(RIF_CImage *image);

static uint8_t rif_int_1_buffer[1];
static uint8_t rif_int_4_buffer[4];

static RIF_Image* librif_image_base(void);

static size_t get_pattern_size_in_bytes(unsigned int patternSize, bool alpha);

static void librif_cimage_read_patterns(RIF_CImage *image, size_t size);
static void librif_cimage_read_cells(RIF_CImage *image, size_t size);

void librif_opaque_get_pixel(RIF_OpaqueImage *image, int x, int y, uint8_t *color, uint8_t *alpha);

static RIF_OpaqueImage* librif_opaque_from_image(RIF_Image *image);
static RIF_OpaqueImage* librif_opaque_from_cimage(RIF_CImage *image);
static void librif_opaque_free(RIF_OpaqueImage *image);

static void* librif_malloc(size_t size);
static void librif_free(void* ptr);

static void librif_gfx_begin_draw(RIF_GFX_Context *context);
static void librif_gfx_end_draw(RIF_GFX_Context *context);

static RIF_GFX_Context librif_gfx_context_new(RIF_GFX_ContextType type);
static void librif_gfx_draw_image_context(RIF_OpaqueImage *image, RIF_GFX_Context *context);

static void librif_gfx_will_draw_pixel(RIF_OpaqueImage *opaqueImage, int x, int y);
static void librif_gfx_draw_pixel(RIF_OpaqueImage *opaqueImage, RIF_GFX_Context *context, uint8_t color, uint8_t alpha, int x, int y, int i, uint8_t d_col, uint8_t d_row, RIF_GFX_ditherFunction ditherFunction);

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
    image->pixels_a = NULL;
    
    #ifdef PLAYDATE
    image->pd_file = NULL;
    #else
    image->stream = NULL;
    #endif
    
    return image;
}

RIF_Image* librif_image_open(const char *filename, RIF_Pool *pool){
    RIF_Image *image = librif_image_base();
    image->pool = pool;
    
    bool isNull = false;
    
    #ifdef PLAYDATE
    image->pd_file = RIF_pd->file->open(filename, kFileRead|kFileReadData);
    
    if(image->pd_file == NULL){
        isNull = true;
    }
    #else
    image->stream = fopen(filename, "rb");
    
    if(image->stream == NULL){
        isNull = true;
    }
    #endif
    
    if(isNull){
        librif_free(image);
        return NULL;
    }

    uint8_t alphaChannelInt = librif_byte_int1(image);
    image->hasAlpha = (alphaChannelInt == 1) ? true : false;

    image->width = librif_byte_int4(image);
    image->height = librif_byte_int4(image);

    size_t numberOfPixels = image->width * image->height;
    
    image->totalBytes = numberOfPixels * sizeof(uint8_t);
    image->readBytes = 0;
    
    if(image->hasAlpha){
        if(pool != NULL){
            image->pixels_a = pool->address;
            pool->address += numberOfPixels * sizeof(RIF_Pixel);
        }
        else {
            image->pixels_a = librif_malloc(numberOfPixels * sizeof(RIF_Pixel));
        }
    }
    else {
        if(pool != NULL){
            image->pixels = pool->address;
            pool->address += numberOfPixels * sizeof(uint8_t);
        }
        else {
            image->pixels = librif_malloc(numberOfPixels * sizeof(uint8_t));
        }
    }
    
    image->opaque = librif_opaque_from_image(image);
    
    return image;
}

bool librif_image_read(RIF_Image *image, size_t size, bool *closed){
    
    if(closed != NULL){
        *closed = false;
    }
    
    bool closeFile = false;
    
    size_t chunk = image->totalBytes;
    if(size > 0){
        chunk = size;
    }
    
    if((image->readBytes + chunk) >= image->totalBytes){
        chunk = image->totalBytes - image->readBytes;
        closeFile = true;
    }
    
    if(image->hasAlpha){
        size_t endRead = image->readBytes + chunk;
        
        for(size_t i = image->readBytes; i < endRead; i++){
            uint8_t alpha = librif_byte_int1(image);

            uint8_t color = 0;
            if(alpha != 0){
                color = librif_byte_int1(image);
            }
            
            image->pixels_a[i] = (RIF_Pixel){
                .color = color,
                .alpha = alpha
            };
        }
    }
    else {
        void *buffer = &image->pixels[image->readBytes];
        
        #ifdef PLAYDATE
        RIF_pd->file->read(image->pd_file, buffer, (unsigned int)chunk);
        #else
        fread(buffer, 1, chunk, image->stream);
        #endif
    }
    
    image->readBytes += chunk;

    if(closeFile){
        if(closed != NULL){
            *closed = true;
        }
                
        #ifdef PLAYDATE
        RIF_pd->file->close(image->pd_file);
        image->pd_file = NULL;
        #else
        fclose(image->stream);
        image->stream = NULL;
        #endif
    }
    
    return true;
}

RIF_Image* librif_image_new(int width, int height){
    
    RIF_Image *image = librif_image_base();
    
    image->hasAlpha = true;
    
    image->width = width;
    image->height = height;
    
    size_t numberOfPixels = image->width * image->height;
    image->pixels_a = librif_malloc(numberOfPixels * sizeof(RIF_Pixel));
    
    for(int i = 0; i < numberOfPixels; i++){
        image->pixels_a[i] = (RIF_Pixel){
            .color = 0,
            .alpha = 0
        };
    }
    
    image->opaque = librif_opaque_from_image(image);
    
    return image;
}

RIF_Image* librif_image_copy(RIF_Image *image){
    
    RIF_Image *copied = librif_image_base();
    
    copied->hasAlpha = image->hasAlpha;
    
    copied->width = image->width;
    copied->height = image->height;

    size_t numberOfPixels = copied->width * copied->height;
    
    if(image->hasAlpha){
        size_t size = numberOfPixels * sizeof(RIF_Pixel);
        copied->pixels_a = librif_malloc(size);
        memcpy(copied->pixels_a, image->pixels_a, size);
    }
    else {
        size_t size = numberOfPixels * sizeof(uint8_t);
        copied->pixels = librif_malloc(size);
        memcpy(copied->pixels, image->pixels, size);
    }
    
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

void librif_image_get_pixel(RIF_Image *image, int x, int y, uint8_t *color, uint8_t *alpha){

    if(x < 0 || x >= image->width || y < 0 || y >= image->height){
        *color = 0;
        if(alpha != NULL){
            *alpha = 255;
        }
        return;
    }

    if(image->hasAlpha){
        RIF_Pixel pixel = image->pixels_a[y * image->width + x];

        *color = pixel.color;
        if(alpha != NULL){
            *alpha = pixel.alpha;
        }
    }
    else {
        *color = image->pixels[y * image->width + x];
        if(alpha != NULL){
            *alpha = 255;
        }
    }
}

void librif_image_set_pixel(RIF_Image *image, int x, int y, uint8_t color, uint8_t alpha){
    if(x >= 0 && x < image->width && y >= 0 && y < image->height){
        unsigned int i = y * image->width + x;
        
        if(image->hasAlpha){
            image->pixels_a[i] = (RIF_Pixel){
                .color = color,
                .alpha = alpha
            };
        }
        else {
            image->pixels[i] = color;
        }
    }
}

RIF_CImage* librif_cimage_open(const char *filename, RIF_Pool *pool){
    RIF_CImage *image = librif_malloc(sizeof(RIF_CImage));
    image->pool = pool;
    
    bool isNull = false;
    
    #ifdef PLAYDATE
    image->pd_file = RIF_pd->file->open(filename, kFileRead|kFileReadData);
    
    if(image->pd_file == NULL){
        isNull = true;
    }
    #else
    image->stream = fopen(filename, "rb");
    
    if(image->stream == NULL){
        isNull = true;
    }
    #endif
    
    if(isNull){
        librif_free(image);
        return NULL;
    }
    
    uint8_t alphaChannelInt = librifc_byte_int1(image);
    image->hasAlpha = (alphaChannelInt == 1) ? true : false;

    image->width = librifc_byte_int4(image);
    image->height = librifc_byte_int4(image);

    unsigned int cx = librifc_byte_int4(image);
    unsigned int cy = librifc_byte_int4(image);

    image->cellCols = cx;
    image->cellRows = cy;

    unsigned int patternSize = librifc_byte_int4(image);
    image->patternSize = patternSize;

    unsigned int numberOfCells = cx * cy;
    image->numberOfCells = numberOfCells;

    unsigned int numberOfPatterns = librifc_byte_int4(image);
    image->numberOfPatterns = numberOfPatterns;

    image->patternsRead = 0;
    image->cellsRead = 0;
    
    size_t patternSizeInBytes = get_pattern_size_in_bytes(patternSize, image->hasAlpha);
    
    image->readBytes = 0;
    image->totalBytes = patternSizeInBytes * numberOfPatterns + numberOfCells * patternIndexInBytes;
    
    image->patterns = NULL;
    image->patterns_a = NULL;
    
    image->cells = NULL;
    image->cells_a = NULL;
    
    int pixelsInPattern = patternSize * patternSize;

    if(image->hasAlpha){
        image->cells_a = librif_malloc(numberOfCells * sizeof(RIF_Cell_A));
        image->patterns_a = librif_malloc(numberOfPatterns * sizeof(RIF_Pattern_A*));
        
        size_t pixelsSizeInBytes = pixelsInPattern * sizeof(RIF_Pixel);
        
        if(pool != NULL){
            image->cells_a = pool->address;
            pool->address += numberOfCells * sizeof(RIF_Cell_A);
            
            image->patterns = pool->address;
            
            pool->address += numberOfPatterns * sizeof(RIF_Pattern_A*);

            for(int i = 0; i < numberOfPatterns; i++){
                void *patternPtr = pool->address;
                pool->address += sizeof(RIF_Pattern_A);
                
                RIF_Pattern_A pattern_a = {
                    .pixels = pool->address
                };
                
                pool->address += pixelsSizeInBytes;
                
                memcpy(patternPtr, &pattern_a, sizeof(RIF_Pattern_A));
                image->patterns_a[i] = patternPtr;
            }
        }
        else {
            for(int i = 0; i < numberOfPatterns; i++){
                RIF_Pattern_A *pattern_a = librif_malloc(sizeof(RIF_Pattern_A));
                pattern_a->pixels = librif_malloc(pixelsSizeInBytes);
                image->patterns_a[i] = pattern_a;
            }
        }
    }
    else {
        size_t pixelsSizeInBytes = pixelsInPattern * sizeof(uint8_t);
        
        if(pool != NULL){
            image->cells = pool->address;
            pool->address += numberOfCells * sizeof(RIF_Cell);
            
            image->patterns = pool->address;
            
            pool->address += numberOfPatterns * sizeof(RIF_Pattern*);

            for(int i = 0; i < numberOfPatterns; i++){
                void *patternPtr = pool->address;
                pool->address += sizeof(RIF_Pattern);
                
                RIF_Pattern pattern = {
                    .pixels = pool->address
                };
                
                pool->address += pixelsSizeInBytes;
                
                memcpy(patternPtr, &pattern, sizeof(RIF_Pattern));
                image->patterns[i] = patternPtr;
            }
        }
        else {
            image->cells = librif_malloc(numberOfCells * sizeof(RIF_Cell));
            image->patterns = librif_malloc(numberOfPatterns * sizeof(RIF_Pattern*));
            
            for(int i = 0; i < numberOfPatterns; i++){
                RIF_Pattern *pattern = librif_malloc(sizeof(RIF_Pattern));
                pattern->pixels = librif_malloc(pixelsSizeInBytes);
                image->patterns[i] = pattern;
            }
        }
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
        if(image->patternsRead < image->numberOfPatterns){
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
        
        #ifdef PLAYDATE
        RIF_pd->file->close(image->pd_file);
        image->pd_file = NULL;
        #else
        fclose(image->stream);
        image->stream = NULL;
        #endif
    }
    
    return true;
}

void librif_cimage_read_patterns(RIF_CImage *image, size_t size){
    
    int numberOfPixels = image->patternSize * image->patternSize;
    
    size_t patternInBytes = numberOfPixels;
    if(image->hasAlpha){
        patternInBytes = numberOfPixels * 2;
    }
    
    int chunk = image->numberOfPatterns;
    if(size > 0){
        chunk = (float)size / patternInBytes;
    }
    
    if(chunk == 0){
        chunk = 1;
    }
    
    if((image->patternsRead + chunk) >= image->numberOfPatterns){
        chunk = image->numberOfPatterns - image->patternsRead;
    }
    
    size_t endRead = image->patternsRead + chunk;
    
    for(size_t i = image->patternsRead; i < endRead; i++){
        if(image->hasAlpha){
            RIF_Pattern_A *pattern_a = image->patterns_a[i];

            for(int j = 0; j < numberOfPixels; j++){
                uint8_t alpha = librifc_byte_int1(image);

                uint8_t color = 0;
                if(alpha != 0){
                    color = librifc_byte_int1(image);
                }
                
                pattern_a->pixels[j] = (RIF_Pixel){
                    .color = color,
                    .alpha = alpha
                };
            }
        }
        else {
            RIF_Pattern *pattern = image->patterns[i];
            void *buffer = pattern->pixels;
            
            #ifdef PLAYDATE
            RIF_pd->file->read(image->pd_file, buffer, numberOfPixels);
            #else
            fread(buffer, 1, numberOfPixels, image->stream);
            #endif
        }
    }
    
    image->patternsRead += chunk;
    image->readBytes += chunk * get_pattern_size_in_bytes(image->patternSize, image->hasAlpha);
}

void librif_cimage_read_cells(RIF_CImage *image, size_t size){
        
    int chunk = image->numberOfCells;
    if(size > 0){
        chunk = (float)size / patternIndexInBytes;
    }
    
    if(chunk == 0){
        chunk = 1;
    }
    
    if((image->cellsRead + chunk) >= image->numberOfCells){
        chunk = image->numberOfCells - image->cellsRead;
    }
        
    int endRead = image->cellsRead + chunk;
    
    for(int i = image->cellsRead; i < endRead; i++){
        uint32_t patternIndex = librifc_byte_int4(image);
        
        if(image->hasAlpha){
            RIF_Pattern_A *pattern_a = image->patterns_a[patternIndex];
            RIF_Cell_A cell_a = {
                .pattern = pattern_a
            };

            image->cells_a[i] = cell_a;
        }
        else {
            RIF_Pattern *pattern = image->patterns[patternIndex];
            RIF_Cell cell = {
                .pattern = pattern
            };

            image->cells[i] = cell;
        }
    }
    
    image->cellsRead += chunk;
    image->readBytes += chunk * patternIndexInBytes;
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
    int pixel_i = patternY * patternSize + patternX;

	if(image->hasAlpha){
		RIF_Cell_A cell_a = image->cells_a[cell_i];
		RIF_Pixel pixel = cell_a.pattern->pixels[pixel_i];

		*color = pixel.color;
        if(alpha != NULL){
            *alpha = pixel.alpha;
        }
	}
	else {
		RIF_Cell cell = image->cells[cell_i];
        
        *color = cell.pattern->pixels[pixel_i];
        if(alpha != NULL){
            *alpha = 255;
        }
	}
}

RIF_Image* librif_cimage_decompress(RIF_CImage *cimage, RIF_Pool *pool){
    
    RIF_Image *image = librif_image_base();
    image->pool = pool;
    
    image->readBytes = 0;
    image->totalBytes = 0;
    
    image->width = cimage->width;
    image->height = cimage->height;
    image->hasAlpha = cimage->hasAlpha;
    
    size_t numberOfPixels = image->width * image->height;
    
    if(image->hasAlpha){
        if(pool != NULL){
            image->pixels_a = pool->address;
            pool->address += numberOfPixels * sizeof(RIF_Pixel);
        }
        else {
            image->pixels_a = librif_malloc(numberOfPixels * sizeof(RIF_Pixel));
        }
    }
    else {
        if(pool != NULL){
            image->pixels = pool->address;
            pool->address += numberOfPixels * sizeof(uint8_t);
        }
        else {
            image->pixels = librif_malloc(numberOfPixels * sizeof(uint8_t));
        }
    }
    
    for(int y = 0; y < image->height; y++){
        for(int x = 0; x < image->width; x++){
            int i = y * image->width + x;
            
            uint8_t color, alpha;
            librif_cimage_get_pixel(cimage, x, y, &color, &alpha);
            
            if(image->hasAlpha){
                image->pixels_a[i] = (RIF_Pixel){
                    .color = color,
                    .alpha = alpha
                };
            }
            else {
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

size_t get_pattern_size_in_bytes(unsigned int patternSize, bool alpha){
    int pixelsInPattern = patternSize * patternSize;
    
    size_t bytes = pixelsInPattern;
    if(alpha){
        bytes = pixelsInPattern * 2;
    }
    return bytes;
}

uint8_t librif_byte_int1(RIF_Image *image){
    #ifdef PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_int_1_buffer, 1);
    #else
    fread(rif_int_1_buffer, 1, 1, image->stream);
    #endif
    return rif_int_1_buffer[0];
}

uint32_t librif_byte_int4(RIF_Image *image){
    #ifdef PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_int_4_buffer, 4);
    #else
    fread(rif_int_4_buffer, 4, 1, image->stream);
    #endif
    return rif_int_4_buffer[0] << 24 | rif_int_4_buffer[1] << 16 | rif_int_4_buffer[2] << 8 | rif_int_4_buffer[3];
}

uint8_t librifc_byte_int1(RIF_CImage *image){
    #ifdef PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_int_1_buffer, 1);
    #else
    fread(rif_int_1_buffer, 1, 1, image->stream);
    #endif
    return rif_int_1_buffer[0];
}

uint32_t librifc_byte_int4(RIF_CImage *image){
    #ifdef PLAYDATE
    RIF_pd->file->read(image->pd_file, rif_int_4_buffer, 4);
    #else
    fread(rif_int_4_buffer, 4, 1, image->stream);
    #endif
    return rif_int_4_buffer[0] << 24 | rif_int_4_buffer[1] << 16 | rif_int_4_buffer[2] << 8 | rif_int_4_buffer[3];
}

void librif_image_free(RIF_Image *image){
    
    if(image->pool == NULL){
        
        if(image->hasAlpha){
            librif_free(image->pixels_a);
        }
        else {
            librif_free(image->pixels);
        }
    }
    
    librif_opaque_free(image->opaque);
    librif_free(image);
}

void librif_cimage_free(RIF_CImage *image){
	
    if(image->pool == NULL){
        uint32_t numberOfPatterns = image->numberOfPatterns;
        bool alpha_channel = image->hasAlpha;

        for(uint32_t i = 0; i < numberOfPatterns; i++){
            if(alpha_channel){
                RIF_Pattern_A *pattern_a = image->patterns_a[i];
                librif_free(pattern_a->pixels);
                librif_free(pattern_a);
            }
            else {
                RIF_Pattern *pattern = image->patterns[i];
                librif_free(pattern->pixels);
                librif_free(pattern);
            }
        }
        
        if(alpha_channel){
            librif_free(image->cells_a);
        }
        else {
            librif_free(image->cells);
        }
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

void librif_opaque_get_pixel(RIF_OpaqueImage *image, int x, int y, uint8_t *color, uint8_t *alpha){
    if(image->compressed){
        librif_cimage_get_pixel((RIF_CImage*)image->image, x, y, color, alpha);
    }
    else {
        librif_image_get_pixel((RIF_Image*)image->image, x, y, color, alpha);
    }
}

#ifdef PLAYDATE
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

        for(int y = 0; y < RIF_LCD_ROWS; y++){
            RIF_bayer2_rows[y] = y % 2;
            RIF_bayer4_rows[y] = y % 4;
            RIF_bayer8_rows[y] = y % 8;
        }
        
        for(int x = 0; x < RIF_LCD_COLUMNS; x++){
            RIF_bayer2_cols[x] = x % 2;
            RIF_bayer4_cols[x] = x % 4;
            RIF_bayer8_cols[x] = x % 8;
        }
        
        #ifdef PLAYDATE
        
        for(int y = 0; y < LCD_ROWS; y++){
            for(int x = 0; x < LCD_COLUMNS; x++){
                int i = y * LCD_COLUMNS + x;
                
                int block_x = x / 8;
                int block_i = y * LCD_ROWSIZE + block_x;
                int n = 7 - x % 8;
                
                uint8_t black_mask = ~(1 << n);
                uint8_t white_mask = (1 << n);
                
                RIF_PD_Pixel pixel = {
                    .i = block_i,
                    .black_mask = black_mask,
                    .white_mask = white_mask
                };
                
                RIF_pd_pixels[i] = pixel;
            }
        }
        
        #endif
    }
}

void librif_gfx_begin_draw(RIF_GFX_Context *context){
    gfx_draw_bounds = RIF_DrawBounds_new();
}

void librif_gfx_end_draw(RIF_GFX_Context *context){
    
    if(context->type == RIF_GFX_ContextTypeLCD){
        #ifdef PLAYDATE
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
    
    #ifdef PLAYDATE
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
    
    float centerX = (float)width * image->centerX_multiplier;
    float centerY = (float)height * image->centerY_multiplier;
    
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
    #ifdef PLAYDATE
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
    
    uint8_t *bayer_rows = RIF_bayer4_rows;
    
    if(gfx_dither_type == RIF_DitherTypeBayer2){
        bayer_rows = RIF_bayer2_rows;
    }
    else if(gfx_dither_type == RIF_DitherTypeBayer8){
        bayer_rows = RIF_bayer8_rows;
    }
    
    uint8_t *bayer_cols = RIF_bayer4_cols;
    
    if(gfx_dither_type == RIF_DitherTypeBayer2){
        bayer_cols = RIF_bayer2_cols;
    }
    else if(gfx_dither_type == RIF_DitherTypeBayer8){
        bayer_cols = RIF_bayer8_cols;
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
        
        float rotation_sin = sinf( - transform.rotation * (M_PI / 180.0f));
        float rotation_cos = cosf( - transform.rotation * (M_PI / 180.0f));

        float d_x = (image->x - transform.centerX) - rect.x - offsetX;
        float d_y = (image->y - transform.centerY) - rect.y - offsetY;
        
        for(int y1 = 0; y1 < rows; y1++){
            int absoluteY = startY + y1;
            
            int buffer_rows = absoluteY * context->cols;
            
            float y_c = y1 - d_y - transform.centerY;
            
            float y_sin = y_c * rotation_sin;
            float y_cos = y_c * rotation_cos;
            
            for(int x1 = 0; x1 < cols; x1++){
                int absoluteX = startX + x1;
                
                float x_c = x1 - d_x - transform.centerX;
                
                int imageX = roundf((x_c * rotation_cos - y_sin + transform.centerX) * transform.scaleX);
                int imageY = roundf((x_c * rotation_sin + y_cos + transform.centerY) * transform.scaleY);
                
                if(imageX >= 0 && imageX < image->width && imageY >= 0 && imageY < image->height){
                    uint8_t color, alpha;
                    librif_opaque_get_pixel(image, imageX, imageY, &color, &alpha);
                    
                    librif_gfx_draw_pixel(image, context, color, alpha, absoluteX, absoluteY, (buffer_rows + absoluteX), bayer_cols[absoluteX], bayer_rows[absoluteY], ditherFunction);
                }
            }
        }
    }
    else {
        
        for(int y1 = 0; y1 < rows; y1++){
            int absoluteY = startY + y1;
            
            uint8_t d_row = bayer_rows[absoluteY];
            int buffer_rows = absoluteY * context->cols;
            
            int imageY = offsetY + y1;
            if(transform.y_scaled){
                imageY = roundf(imageY * transform.scaleY);
            }
            
            for(int x1 = 0; x1 < cols; x1++){
                int absoluteX = startX + x1;
                
                uint8_t d_col = bayer_cols[absoluteX];
                
                int imageX = offsetX + x1;
                if(transform.x_scaled){
                    imageX = roundf(imageX * transform.scaleX);
                }
                
                if(imageX >= 0 && imageX < image->width && imageY >= 0 && imageY < image->height){
                    uint8_t color, alpha;
                    librif_opaque_get_pixel(image, imageX, imageY, &color, &alpha);
                    
                    librif_gfx_draw_pixel(image, context, color, alpha, absoluteX, absoluteY, (buffer_rows + absoluteX), d_col, d_row, ditherFunction);
                }
            }
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

void librif_gfx_draw_pixel(RIF_OpaqueImage *opaqueImage, RIF_GFX_Context *context, uint8_t color, uint8_t alpha, int x, int y, int i, uint8_t d_col, uint8_t d_row, RIF_GFX_ditherFunction ditherFunction){
    
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
                
                uint8_t ditherColor = ditherFunction(d_col, d_row);
                
                uint8_t adjustedColor = color;
                
                if(opaqueImage->hasAlpha){
                    adjustedColor = color * alpha / 255.0f;
                }
                
                if(context->type == RIF_GFX_ContextTypeLCD){
                    #ifdef PLAYDATE
                    RIF_PD_Pixel pixel = RIF_pd_pixels[i];
                    
                    if(adjustedColor < ditherColor){
                        // black
                        context->pd_framebuffer[pixel.i] &= pixel.black_mask;
                    }
                    else {
                        // white
                        context->pd_framebuffer[pixel.i] |= pixel.white_mask;
                    }
                    #endif
                }
                else if(context->type == RIF_GFX_ContextTypeBitmap){
                    
                    #ifdef PLAYDATE
                    RIF_pd->graphics->fillRect(x, y, 1, 1, (adjustedColor < ditherColor) ? kColorBlack : kColorWhite);
                    #endif
                }
            }
        }
        else if(context->type == RIF_GFX_ContextTypeImage){
            librif_gfx_will_draw_pixel(opaqueImage, x, y);
            
            RIF_Image *dstImage = context->dstImage;
            
            if(dstImage->hasAlpha){
                RIF_Pixel *pixel = &dstImage->pixels_a[i];
                
                uint8_t result_color = color;
                uint8_t result_alpha = alpha;
                
                if(alpha < 255){
                    int alpha_inverse = 255 - (int)alpha;
                    result_color = (alpha_inverse * pixel->color + alpha * color) / 255.0f;
                    result_alpha = 255 - ((255 - pixel->alpha) * alpha_inverse / 255.0f);
                }
                
                pixel->color = result_color;
                pixel->alpha = result_alpha;
            }
            else {
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
    
    float rotation_sin = sinf(transform->rotation * (M_PI / 180.0f));
    float rotation_cos = cosf(transform->rotation * (M_PI / 180.0f));
    
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

#ifdef PLAYDATE

void* librif_malloc(size_t size) {
    return RIF_pd->system->realloc(NULL, size);
}

void librif_free(void* ptr) {
    RIF_pd->system->realloc(ptr, 0);
}

#else

void* librif_malloc(size_t size) {
    return malloc(size);
}

void librif_free(void* ptr) {
    free(ptr);
}

#endif
