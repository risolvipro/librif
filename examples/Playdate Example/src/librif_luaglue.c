//
//  librif_luaglue.c
//  Librif Playdate example
//
//  Created by Matteo D'Ignazio on 11/03/22.
//

#include "librif_luaglue.h"

static void* getObject(int n, char* type);

static RIF_Image* getImage(int n);
static RIF_CImage* getCImage(int n);
static RIF_Pool* getPool(int n);

static const lua_reg librif_image[];
static const lua_reg librif_cimage[];
static const lua_reg librif_pool[];

static char *kImageClass = "librif.image";
static char *kCImageClass = "librif.cimage";
static char *kPoolClass = "librif.pool";

static int graphics_init(lua_State *L);
static int graphics_setDitherType(lua_State *L);
static int graphics_setBlendColor(lua_State *L);
static int graphics_clearBlendColor(lua_State *L);
static int graphics_getDrawBounds(lua_State *L);

void librif_lua_register(void){
    
    const char *err;
    
    if(!RIF_pd->lua->addFunction(graphics_init, "librif.graphics.init", &err)){
        RIF_pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->addFunction(graphics_setDitherType, "librif.graphics.setDitherType", &err)){
        RIF_pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->addFunction(graphics_setBlendColor, "librif.graphics.setBlendColor", &err)){
        RIF_pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->addFunction(graphics_clearBlendColor, "librif.graphics.clearBlendColor", &err)){
        RIF_pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->addFunction(graphics_getDrawBounds, "librif.graphics.getDrawBounds", &err)){
        RIF_pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->registerClass(kImageClass, librif_image, NULL, 0, &err)){
        RIF_pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->registerClass(kCImageClass, librif_cimage, NULL, 0, &err)){
        RIF_pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
    }
    
    if(!RIF_pd->lua->registerClass(kPoolClass, librif_pool, NULL, 0, &err)){
        RIF_pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
    }
}

static int pool_new(lua_State *L){
    
    size_t size = RIF_pd->lua->getArgFloat(1);
    
    RIF_Pool *pool = librif_pool_new(size);
    
    LuaUDObject *UDObject = RIF_pd->lua->pushObject(pool, kPoolClass, 0);
    RIF_pd->lua->retainObject(UDObject);
    
    return 1;
}

static int pool_clear(lua_State *L){
    RIF_Pool *pool = getPool(1);
    librif_pool_clear(pool);
    
    return 0;
}

static int pool_release(lua_State *L){
    LuaUDObject *UDObject = NULL;
    RIF_pd->lua->getArgObject(1, kPoolClass, &UDObject);
    
    if(UDObject != NULL){
        RIF_pd->lua->releaseObject(UDObject);
    }
    
    return 0;
}

static int pool_gc(lua_State *L){
    RIF_Pool *pool = getPool(1);
    librif_pool_free(pool);
    
    return 0;
}

static const lua_reg librif_pool[] = {
    { "__gc", pool_gc },
    { "new", pool_new },
    { "clear", pool_clear },
    { "release", pool_release },
    { NULL, NULL }
};

static int image_open(lua_State *L){
    const char *filename = RIF_pd->lua->getArgString(1);
    
    RIF_Pool *pool = NULL;
    
    void *poolArg = RIF_pd->lua->getArgObject(2, kPoolClass, NULL);
    if(poolArg != NULL){
        pool = poolArg;
    }
    
    RIF_Image *image = librif_image_open(filename, pool);
    
    if(image != NULL){
        RIF_pd->lua->pushObject(image, kImageClass, 0);
    }
    else {
        RIF_pd->lua->pushNil();
    }
    
    return 1;
}

static int image_read(lua_State *L){
    RIF_Image *image = getImage(1);
    int size = RIF_pd->lua->getArgFloat(2);
    
    bool closed;
    bool success = librif_image_read(image, size, &closed);
    
    RIF_pd->lua->pushBool(success ? 1 : 0);
    RIF_pd->lua->pushBool(closed ? 1 : 0);

    return 2;
}

static int image_getWidth(lua_State *L){
    RIF_Image *image = getImage(1);
    RIF_pd->lua->pushInt(image->width);
    
    return 1;
}

static int image_getHeight(lua_State *L){
    RIF_Image *image = getImage(1);
    RIF_pd->lua->pushInt(image->height);
    
    return 1;
}

static int image_hasAlpha(lua_State *L){
    RIF_Image *image = getImage(1);
    RIF_pd->lua->pushBool(image->hasAlpha ? 1 : 0);
    
    return 1;
}

static int image_getReadBytes(lua_State *L){
    RIF_Image *image = getImage(1);
    RIF_pd->lua->pushInt((unsigned int)image->readBytes);
    
    return 1;
}

static int image_getTotalBytes(lua_State *L){
    RIF_Image *image = getImage(1);
    RIF_pd->lua->pushInt((unsigned int)image->totalBytes);
    
    return 1;
}

static int image_getPixel(lua_State *L){
    RIF_Image *image = getImage(1);
    
    int x = RIF_pd->lua->getArgFloat(2);
    int y = RIF_pd->lua->getArgFloat(3);
    
    uint8_t color, alpha;
    librif_image_get_pixel(image, x, y, &color, &alpha);
    
    RIF_pd->lua->pushInt(color);
    RIF_pd->lua->pushInt(alpha);
    
    return 2;
}

static int image_setPosition(lua_State *L){
    RIF_Image *image = getImage(1);
    
    int x = RIF_pd->lua->getArgFloat(2);
    int y = RIF_pd->lua->getArgFloat(3);
    
    librif_opaque_set_position(image->opaque, x, y);
    
    return 0;
}

static int image_setCenter(lua_State *L){
    RIF_Image *cimage = getImage(1);
    
    float x_multiplier = RIF_pd->lua->getArgFloat(2);
    float y_multiplier = RIF_pd->lua->getArgFloat(3);
    
    librif_opaque_set_center(cimage->opaque, x_multiplier, y_multiplier);
    
    return 0;
}

static int image_setSize(lua_State *L){
    RIF_Image *image = getImage(1);
    
    int width = RIF_pd->lua->getArgFloat(2);
    int height = RIF_pd->lua->getArgFloat(3);
    
    librif_opaque_set_size(image->opaque, width, height);
    
    return 0;
}

static int image_setRotation(lua_State *L){
    RIF_Image *image = getImage(1);
    
    int angle = RIF_pd->lua->getArgFloat(2);
    
    librif_opaque_set_rotation(image->opaque, angle);
    
    return 0;
}

static int image_draw(lua_State *L){
    RIF_Image *image = getImage(1);

    librif_gfx_draw_image(image->opaque);
    
    return 0;
}

static int image_draw_into(lua_State *L){
    RIF_Image *source = getImage(1);
    RIF_Image *destination = getImage(2);
    
    librif_gfx_draw_image_into(source->opaque, destination);
    
    return 0;
}

static int image_transform(lua_State *L){
    RIF_Image *source = getImage(1);

    RIF_Image *image = librif_image_transform(source->opaque);
    
    RIF_pd->lua->pushObject(image, kImageClass, 0);
    
    return 1;
}

static int image_to_bitmap(lua_State *L){
    RIF_Image *image = getImage(1);
    
    LCDBitmap *bitmap = librif_opaque_image_to_bitmap(image->opaque);
    
    RIF_pd->lua->pushBitmap(bitmap);
    
    return 1;
}

static int image_copy(lua_State *L){
    RIF_Image *image = getImage(1);
    
    RIF_Image *copied = librif_image_copy(image);
    
    RIF_pd->lua->pushObject(copied, kImageClass, 0);
    
    return 1;
}

static int image_gc(lua_State *L){
    RIF_Image *image = getImage(0);
    librif_image_free(image);
    
    return 0;
}

static const lua_reg librif_image[] = {
    { "__gc", image_gc },
    { "open", image_open },
    { "read", image_read },
    { "getWidth", image_getWidth },
    { "getHeight", image_getHeight },
    { "hasAlpha", image_hasAlpha },
    { "getPixel", image_getPixel },
    { "getReadBytes", image_getReadBytes },
    { "getTotalBytes", image_getTotalBytes },
    // manipulation
    { "setPosition", image_setPosition },
    { "setCenter", image_setCenter },
    { "setSize", image_setSize },
    { "setRotation", image_setRotation },
    // drawing
    { "draw", image_draw },
    { "drawInto", image_draw_into },
    { "transform", image_transform },
    { "toBitmap", image_to_bitmap },
    // copy
    { "copy", image_copy },
    { NULL, NULL }
};

//
// Compressed
//

static int cimage_open(lua_State *L){
    const char *filename = RIF_pd->lua->getArgString(1);
    
    RIF_Pool *pool = NULL;
    
    void *poolArg = RIF_pd->lua->getArgObject(2, kPoolClass, NULL);
    if(poolArg != NULL){
        pool = poolArg;
    }
    
    RIF_CImage *image = librif_cimage_open(filename, pool);
    
    if(image != NULL){
        RIF_pd->lua->pushObject(image, kCImageClass, 0);
    }
    else {
        RIF_pd->lua->pushNil();
    }
    
    return 1;
}

static int cimage_read(lua_State *L){
    RIF_CImage *image = getCImage(1);
    int size = RIF_pd->lua->getArgFloat(2);
    
    bool closed;
    bool success = librif_cimage_read(image, size, &closed);
    
    RIF_pd->lua->pushBool(success ? 1 : 0);
    RIF_pd->lua->pushBool(closed ? 1 : 0);

    return 2;
}

static int cimage_getWidth(lua_State *L){
    RIF_CImage *image = getCImage(1);
    RIF_pd->lua->pushInt(image->width);
    
    return 1;
}

static int cimage_getHeight(lua_State *L){
    RIF_CImage *image = getCImage(1);
    RIF_pd->lua->pushInt(image->height);
    
    return 1;
}

static int cimage_hasAlpha(lua_State *L){
    RIF_CImage *image = getCImage(1);
    RIF_pd->lua->pushBool(image->hasAlpha ? 1 : 0);
    
    return 1;
}

static int cimage_getReadBytes(lua_State *L){
    RIF_CImage *image = getCImage(1);
    RIF_pd->lua->pushInt((unsigned int)image->readBytes);
    
    return 1;
}

static int cimage_getTotalBytes(lua_State *L){
    RIF_CImage *image = getCImage(1);
    RIF_pd->lua->pushInt((unsigned int)image->totalBytes);
    
    return 1;
}

static int cimage_getPixel(lua_State *L){
    RIF_CImage *image = getCImage(1);
    
    int x = RIF_pd->lua->getArgFloat(2);
    int y = RIF_pd->lua->getArgFloat(3);
    
    uint8_t color, alpha;
    librif_cimage_get_pixel(image, x, y, &color, &alpha);
    
    RIF_pd->lua->pushInt(color);
    RIF_pd->lua->pushInt(alpha);
    
    return 2;
}
    
static int cimage_decompress(lua_State *L){
    RIF_CImage *cimage = getCImage(1);

    RIF_Pool *pool = NULL;
    
    void *poolArg = RIF_pd->lua->getArgObject(2, kPoolClass, NULL);
    if(poolArg != NULL){
        pool = poolArg;
    }
    
    RIF_Image *image = librif_cimage_decompress(cimage, pool);
    
    RIF_pd->lua->pushObject(image, kCImageClass, 0);
    
    return 1;
}

static int cimage_setPosition(lua_State *L){
    RIF_CImage *cimage = getCImage(1);
    
    int x = RIF_pd->lua->getArgFloat(2);
    int y = RIF_pd->lua->getArgFloat(3);
    
    librif_opaque_set_position(cimage->opaque, x, y);
    
    return 0;
}

static int cimage_setCenter(lua_State *L){
    RIF_CImage *cimage = getCImage(1);
    
    float x_multiplier = RIF_pd->lua->getArgFloat(2);
    float y_multiplier = RIF_pd->lua->getArgFloat(3);
    
    librif_opaque_set_center(cimage->opaque, x_multiplier, y_multiplier);
    
    return 0;
}

static int cimage_setSize(lua_State *L){
    RIF_CImage *cimage = getCImage(1);
    
    int width = RIF_pd->lua->getArgFloat(2);
    int height = RIF_pd->lua->getArgFloat(3);
    
    librif_opaque_set_size(cimage->opaque, width, height);
    
    return 0;
}

static int cimage_setRotation(lua_State *L){
    RIF_CImage *cimage = getCImage(1);
    
    int angle = RIF_pd->lua->getArgFloat(2);
    
    librif_opaque_set_rotation(cimage->opaque, angle);
    
    return 0;
}
    
static int cimage_draw(lua_State *L){
    RIF_CImage *image = getCImage(1);

    librif_gfx_draw_image(image->opaque);
    
    return 0;
}

static int cimage_draw_into(lua_State *L){
    RIF_CImage *source = getCImage(1);
    RIF_Image *destination = getImage(2);
    
    librif_gfx_draw_image_into(source->opaque, destination);
    
    return 0;
}

static int cimage_transform(lua_State *L){
    RIF_CImage *source = getCImage(1);

    RIF_Image *image = librif_image_transform(source->opaque);
    
    RIF_pd->lua->pushObject(image, kImageClass, 0);
    
    return 1;
}

static int cimage_to_bitmap(lua_State *L){
    RIF_CImage *image = getCImage(1);
    
    LCDBitmap *bitmap = librif_opaque_image_to_bitmap(image->opaque);
    
    RIF_pd->lua->pushBitmap(bitmap);
    
    return 1;
}

static int cimage_gc(lua_State *L){
    RIF_CImage *image = getCImage(0);
    librif_cimage_free(image);
    
    return 0;
}

static const lua_reg librif_cimage[] = {
    { "__gc", cimage_gc },
    { "open", cimage_open },
    { "read", cimage_read },
    { "getWidth", cimage_getWidth },
    { "getHeight", cimage_getHeight },
    { "hasAlpha", cimage_hasAlpha },
    { "getPixel", cimage_getPixel },
    { "getReadBytes", cimage_getReadBytes },
    { "getTotalBytes", cimage_getTotalBytes },
    { "decompress", cimage_decompress },
    // manipulation
    { "setPosition", cimage_setPosition },
    { "setCenter", cimage_setCenter },
    { "setSize", cimage_setSize },
    { "setRotation", cimage_setRotation },
    // drawing
    { "draw", cimage_draw },
    { "drawInto", cimage_draw_into },
    { "transform", cimage_transform },
    { "toBitmap", cimage_to_bitmap },
    { NULL, NULL }
};

static int graphics_init(lua_State *L) {
    
    librif_gfx_init();
    
    return 0;
}

static int graphics_setDitherType(lua_State *L) {
    int typeInt = RIF_pd->lua->getArgInt(1);
    
    RIF_DitherType type = RIF_DitherTypeBayer4;
    
    if(typeInt == 0){
        type = RIF_DitherTypeBayer2;
    }
    else if(typeInt == 2){
        type = RIF_DitherTypeBayer8;
    }
    
    librif_gfx_set_dither_type(type);
    
    return 0;
}

static int graphics_setBlendColor(lua_State *L) {

    int color = RIF_pd->lua->getArgFloat(1);
    librif_gfx_set_blend_color(color);
    
    return 0;
}

static int graphics_clearBlendColor(lua_State *L) {
    
    librif_gfx_clear_blend_color();
    return 0;
}

static int graphics_getDrawBounds(lua_State *L) {
    
    RIF_Rect bounds = librif_gfx_get_draw_bounds();
    
    RIF_pd->lua->pushInt(bounds.x);
    RIF_pd->lua->pushInt(bounds.y);
    RIF_pd->lua->pushInt(bounds.width);
    RIF_pd->lua->pushInt(bounds.height);
    
    return 4;
}

static void* getObject(int n, char* type){
    void *object = RIF_pd->lua->getArgObject(n, type, NULL);
    
    if(object == NULL){
        RIF_pd->system->error("object of type %s not found at stack position %i", type, n);
    }
    
    return object;
}

static RIF_Image* getImage(int n){
    return getObject(n, kImageClass);
}

static RIF_CImage* getCImage(int n){
    return getObject(n, kCImageClass);
}

static RIF_Pool* getPool(int n){
    return getObject(n, kPoolClass);
}
