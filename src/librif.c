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

#define RIF_LCD_ROWS 0
#define RIF_LCD_COLUMNS 0

#endif

static bool gfx_init_flag = false;
static RIF_DitherType gfx_dither_type = RIF_DitherTypeBayer4;

static uint8_t gfx_blend_color = 0;
static bool gfx_has_blend_color = false;

static const size_t patternIndexInBytes = 4;

#ifdef PLAYDATE

typedef struct {
    uint16_t i;
    uint8_t n;
} RIF_PD_Pixel;

static uint8_t RIF_pd_bayer2_rows[LCD_ROWS];
static uint8_t RIF_pd_bayer2_cols[LCD_COLUMNS];

static uint8_t RIF_pd_bayer4_rows[LCD_ROWS];
static uint8_t RIF_pd_bayer4_cols[LCD_COLUMNS];

static uint8_t RIF_pd_bayer8_rows[LCD_ROWS];
static uint8_t RIF_pd_bayer8_cols[LCD_COLUMNS];

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

uint8_t librif_byte_int1(RIF_Image *image);
uint32_t librif_byte_int4(RIF_Image *image);

uint8_t librifc_byte_int1(RIF_CImage *image);
uint32_t librifc_byte_int4(RIF_CImage *image);

uint8_t rif_int_1_buffer[1];
uint8_t rif_int_4_buffer[4];

static size_t get_pattern_size_in_bytes(uint32_t patternSize, bool alpha);

void librif_cimage_read_patterns(RIF_CImage *image, size_t size);
void librif_cimage_read_cells(RIF_CImage *image, size_t size);

void librif_opaque_get_pixel(RIF_OpaqueImage *image, uint32_t x, uint32_t y, uint8_t *color, uint8_t *alpha);

static RIF_OpaqueImage* librif_opaque_from_image(RIF_Image *image);
static RIF_OpaqueImage* librif_opaque_from_cimage(RIF_CImage *image);
static void librif_opaque_free(RIF_OpaqueImage *image);

void* librif_malloc(size_t size);
void* librif_calloc(size_t count, size_t size);
void librif_free(void* ptr);

RIF_Image* librif_image_open(const char *filename, RIF_Pool *pool){
    RIF_Image *image = librif_malloc(sizeof(RIF_Image));
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

    unsigned int numberOfPixels = image->width * image->height;
    
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

            RIF_Pixel pixel_a = {
                .color = color,
                .alpha = alpha
            };
            image->pixels_a[i] = pixel_a;
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
        #else
        fclose(image->stream);
        #endif
    }
    
    return true;
}

void librif_image_get_pixel(RIF_Image *image, uint32_t x, uint32_t y, uint8_t *color, uint8_t *alpha) {

    if(x >= image->width || y >= image->height){
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

    uint32_t cx = librifc_byte_int4(image);
    uint32_t cy = librifc_byte_int4(image);

    image->cellCols = cx;
    image->cellRows = cy;

    uint32_t patternSize = librifc_byte_int4(image);
    image->patternSize = patternSize;

    uint32_t numberOfCells = cx * cy;
    image->numberOfCells = numberOfCells;

    uint32_t numberOfPatterns = librifc_byte_int4(image);
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
    
    uint32_t pixelsInPattern = patternSize * patternSize;

    if(image->hasAlpha){
        image->cells_a = librif_malloc(numberOfCells * sizeof(RIF_Cell_A));
        image->patterns_a = librif_malloc(numberOfPatterns * sizeof(RIF_Pattern_A*));
        
        size_t pixelsSizeInBytes = pixelsInPattern * sizeof(RIF_Pixel);
        
        if(pool != NULL){
            image->cells_a = pool->address;
            pool->address += numberOfCells * sizeof(RIF_Cell_A);
            
            image->patterns = pool->address;
            
            pool->address += numberOfPatterns * sizeof(RIF_Pattern_A*);

            for(uint32_t i = 0; i < numberOfPatterns; i++){
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
            for(uint32_t i = 0; i < numberOfPatterns; i++){
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

            for(uint32_t i = 0; i < numberOfPatterns; i++){
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
            
            for(uint32_t i = 0; i < numberOfPatterns; i++){
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
        #else
        fclose(image->stream);
        #endif
    }
    
    return true;
}

void librif_cimage_read_patterns(RIF_CImage *image, size_t size){
    
    uint32_t pixelsInPattern = image->patternSize * image->patternSize;
    
    size_t patternBytes = pixelsInPattern * 1;
    if(image->hasAlpha){
        patternBytes = pixelsInPattern * 2;
    }
    
    uint32_t chunk = image->numberOfPatterns;
    if(size > 0){
        chunk = (float)size / patternBytes;
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

            for(uint32_t j = 0; j < pixelsInPattern; j++){
                uint8_t alpha = librifc_byte_int1(image);

                uint8_t color = 0;
                if(alpha != 0){
                    color = librifc_byte_int1(image);
                }

                RIF_Pixel pixel = {
                    .color = color,
                    .alpha = alpha
                };
                pattern_a->pixels[j] = pixel;
            }
        }
        else {
            RIF_Pattern *pattern = image->patterns[i];
            void *buffer = pattern->pixels;
            
            #ifdef PLAYDATE
            RIF_pd->file->read(image->pd_file, buffer, pixelsInPattern);
            #else
            fread(buffer, 1, pixelsInPattern, image->stream);
            #endif
        }
    }
    
    image->patternsRead += chunk;
    image->readBytes += chunk * get_pattern_size_in_bytes(image->patternSize, image->hasAlpha);
}

void librif_cimage_read_cells(RIF_CImage *image, size_t size){
        
    uint32_t chunk = image->numberOfCells;
    if(size > 0){
        chunk = floorf((float)size / patternIndexInBytes);
    }
    
    if(chunk == 0){
        chunk = 1;
    }
    
    if((image->cellsRead + chunk) >= image->numberOfCells){
        chunk = image->numberOfCells - image->cellsRead;
    }
        
    uint32_t endRead = image->cellsRead + chunk;
    
    for(uint32_t i = image->cellsRead; i < endRead; i++){
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

void librif_cimage_get_pixel(RIF_CImage *image, uint32_t x, uint32_t y, uint8_t *color, uint8_t *alpha) {

	if(x >= image->width || y >= image->height){
		*color = 0;
        if(alpha != NULL){
            *alpha = 255;
        }
		return;
	}

	uint32_t patternSize = image->patternSize;

	uint32_t cellCol = x / patternSize;
	uint32_t cellRow = y / patternSize;

	uint32_t patternX = x - cellCol * patternSize;
	uint32_t patternY = y - cellRow * patternSize;

	uint32_t cell_i = cellRow * image->cellCols + cellCol;
	uint32_t pixel_i = patternY * patternSize + patternX;

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
    
    RIF_Image *image = librif_malloc(sizeof(RIF_Image));
    image->pool = pool;
    
    image->readBytes = 0;
    image->totalBytes = 0;
    
    image->width = cimage->width;
    image->height = cimage->height;
    image->hasAlpha = cimage->hasAlpha;
    
    unsigned int numberOfPixels = image->width * image->height;
    
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
    
    for(uint32_t y = 0; y < image->height; y++){
        for(uint32_t x = 0; x < image->width; x++){
            unsigned int i = y * image->width + x;
            
            uint8_t color, alpha;
            librif_cimage_get_pixel(cimage, x, y, &color, &alpha);
            
            if(image->hasAlpha){
                RIF_Pixel pixel = {
                    .color = color,
                    .alpha = alpha
                };
                image->pixels_a[i] = pixel;
            }
            else {
                image->pixels[i] = color;
            }
        }
    }
    
    image->opaque = librif_opaque_from_image(image);
    
    return image;
}

static RIF_OpaqueImage* librif_opaque_from_image(RIF_Image *image){
    RIF_OpaqueImage *opaqueImage = librif_malloc(sizeof(RIF_OpaqueImage));
    opaqueImage->compressed = false;
    opaqueImage->image = image;
    opaqueImage->width = image->width;
    opaqueImage->height = image->height;
    opaqueImage->hasAlpha = image->hasAlpha;
    
    return opaqueImage;
}

static RIF_OpaqueImage* librif_opaque_from_cimage(RIF_CImage *image){
    RIF_OpaqueImage *opaqueImage = librif_malloc(sizeof(RIF_OpaqueImage));
    opaqueImage->compressed = true;
    opaqueImage->image = image;
    opaqueImage->width = image->width;
    opaqueImage->height = image->height;
    opaqueImage->hasAlpha = image->hasAlpha;
    
    return opaqueImage;
}

static void librif_opaque_free(RIF_OpaqueImage *image){
    
    librif_free(image);
}

size_t get_pattern_size_in_bytes(uint32_t patternSize, bool alpha){
    uint32_t pixelsInPattern = patternSize * patternSize;
    
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

void librif_opaque_get_pixel(RIF_OpaqueImage *image, uint32_t x, uint32_t y, uint8_t *color, uint8_t *alpha){
    if(image->compressed){
        librif_cimage_get_pixel((RIF_CImage*)image->image, x, y, color, alpha);
    }
    else {
        librif_image_get_pixel((RIF_Image*)image->image, x, y, color, alpha);
    }
}

// Drawing functions

void librif_gfx_init(void){
    
    if(!gfx_init_flag){
        gfx_init_flag = true;
        
        #ifdef PLAYDATE
        
        for(int y = 0; y < LCD_ROWS; y++){
            RIF_pd_bayer2_rows[y] = y % 2;
            RIF_pd_bayer4_rows[y] = y % 4;
            RIF_pd_bayer8_rows[y] = y % 8;
            
            for(int x = 0; x < LCD_COLUMNS; x++){
                RIF_pd_bayer2_cols[x] = x % 2;
                RIF_pd_bayer4_cols[x] = x % 4;
                RIF_pd_bayer8_cols[x] = x % 8;
                
                int i = y * LCD_COLUMNS + x;
                
                int block_x = x / 8;
                int block_i = y * LCD_ROWSIZE + block_x;
                int bit_n = (block_x + 1) * 8 - 1 - x;
                
                RIF_PD_Pixel pixel = {
                    .i = block_i,
                    .n = bit_n
                };
                
                RIF_pd_pixels[i] = pixel;
            }
        }
        
        #endif
    }
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

void librif_gfx_draw_image(RIF_OpaqueImage *image, int x, int y) {
    librif_gfx_init();
    
    librif_gfx_draw_scaled_image(image, x, y, image->width, image->height);
}

void librif_gfx_draw_scaled_image(RIF_OpaqueImage *image, int x, int y, unsigned int width, unsigned int height){
    librif_gfx_init();
    
    bool x_scaled = false;
    bool y_scaled = false;

    float scaleX = 1;
    float scaleY = 1;
    
    if(image->width != width){
        x_scaled = true;
        
        scaleX = 0;
        if(width != 0){
            scaleX = (float)image->width / width;
        }
    }
    
    if(image->height != height){
        y_scaled = true;
        
        scaleY = 0;
        if(height != 0){
            scaleY = (float)image->height / height;
        }
    }
    
    int ox = 0;
    int cx = x;
    
    if(x < 0){
        ox = x;
        cx = 0;
    }
    
    int oy = 0;
    int cy = y;
    
    if(y < 0){
        oy = y;
        cy = 0;
    }
    
    int cw = RIF_MIN(width - ox, RIF_LCD_COLUMNS);
    int ch = RIF_MIN(height - oy, RIF_LCD_ROWS);
    
    #ifdef PLAYDATE
    uint8_t *framebuffer = RIF_pd->graphics->getFrame();
    #endif
    
    uint8_t *bayer_rows = NULL;
    uint8_t *bayer_cols = NULL;
    
    #ifdef PLAYDATE
    bayer_rows = RIF_pd_bayer4_rows;
    
    if(gfx_dither_type == RIF_DitherTypeBayer2){
        bayer_rows = RIF_pd_bayer2_rows;
    }
    else if(gfx_dither_type == RIF_DitherTypeBayer8){
        bayer_rows = RIF_pd_bayer8_rows;
    }
    #endif
    
    #ifdef PLAYDATE
    bayer_cols = RIF_pd_bayer4_cols;
    
    if(gfx_dither_type == RIF_DitherTypeBayer2){
        bayer_cols = RIF_pd_bayer2_cols;
    }
    else if(gfx_dither_type == RIF_DitherTypeBayer8){
        bayer_cols = RIF_pd_bayer8_cols;
    }
    #endif
    
    for(unsigned int y1 = cy; y1 < ch; y1++){
        uint8_t d_row = bayer_rows[y1];
        
        #ifdef PLAYDATE
        unsigned int lcd_rows = y1 * RIF_LCD_COLUMNS;
        #endif
        
        uint32_t sourceY = y1 - oy;
        if(y_scaled){
            sourceY = roundf(sourceY * scaleY);
        }
        
        for(unsigned int x1 = cx; x1 < cw; x1++){
            uint8_t d_col = bayer_cols[x1];
            
            uint32_t sourceX = x1 - ox;
            if(x_scaled){
                sourceX = roundf(sourceX * scaleX);
            }
            
            uint8_t color, alpha;
            librif_opaque_get_pixel(image, sourceX, sourceY, &color, &alpha);
            
            bool drawPixel = true;
            
            if(image->hasAlpha){
                if(alpha < 128){
                    drawPixel = false;
                }
            }
            else if(gfx_has_blend_color && color == gfx_blend_color){
                drawPixel = false;
            }
            
            if(drawPixel){
                uint8_t ditherColor = 0;
                
                if(gfx_dither_type == RIF_DitherTypeBayer2){
                    ditherColor = RIF_bayer2[d_col][d_row];
                }
                else if(gfx_dither_type == RIF_DitherTypeBayer4){
                    ditherColor = RIF_bayer4[d_col][d_row];
                }
                else if(gfx_dither_type == RIF_DitherTypeBayer8){
                    ditherColor = RIF_bayer8[d_col][d_row];
                }
                
                #ifdef PLAYDATE

                RIF_PD_Pixel pixel = RIF_pd_pixels[lcd_rows + x1];
                
                if(color < ditherColor){
                    // black
                    framebuffer[pixel.i] &= ~(1 << pixel.n);
                }
                else {
                    // white
                    framebuffer[pixel.i] |= (1 << pixel.n);
                }
                
                #endif
            }
        }
    }
    
    #ifdef PLAYDATE
    RIF_pd->graphics->markUpdatedRows(cy, cy + ch);
    #endif
}

#ifdef PLAYDATE

void* librif_malloc(size_t size) {
    return RIF_pd->system->realloc(NULL, size);
}

void* librif_calloc(size_t count, size_t size) {
    return memset(librif_malloc(count * size), 0, count * size);
}

void librif_free(void* ptr) {
    RIF_pd->system->realloc(ptr, 0);
}

#else

void* librif_malloc(size_t size) {
    return malloc(size);
}

void* librif_calloc(size_t count, size_t size) {
    return calloc(count, size);
}

void librif_free(void* ptr) {
    free(ptr);
}

#endif
