//
//  main.c
//  Librif example
//
//  Created by Matteo D'Ignazio on 11/03/22.
//

#include <stdio.h>

#include "librif.h"

char *filename1 = "../../images/track-1024-raw.rif";
char *filename2 = "../../images/track-1024-compressed.rif";

void read_image(void) {
    
    RIF_Image *image = librif_image_open(filename1, NULL);
    if(image != NULL){
        bool success = librif_image_read(image, 0, NULL);
        
        if(success){
            // successfully read
            
            int x = 0, y = 0;
            
            uint8_t color, alpha;
            librif_image_get_pixel(image, x, y, &color, &alpha);
            
            printf("color %u, alpha %u at %u %u \n", color, alpha, x, y);
        }
    }
}

void read_image_chunk(void) {

    RIF_Image *image = librif_image_open(filename1, NULL);
    if(image != NULL){
        
        while(1){
            bool closed;
            bool success = librif_image_read(image, (50 * 1000), &closed);
            
            if(success){
                printf("read bytes %zu of %zu \n", image->readBytes, image->totalBytes);
            }
            
            if(!success || closed){
                break;
            }
        }
        
        int x = 0, y = 0;
        
        uint8_t color, alpha;
        librif_image_get_pixel(image, x, y, &color, &alpha);
        
        printf("color %u, alpha %u at %u %u \n", color, alpha, x, y);
    }
}

void read_cimage(void) {
        
    RIF_CImage *image = librif_cimage_open(filename2, NULL);
    if(image != NULL){
        bool success = librif_cimage_read(image, 0, NULL);
        
        if(success){
            // successfully read
            
            int x = 0, y = 0;
            
            uint8_t color, alpha;
            librif_cimage_get_pixel(image, x, y, &color, &alpha);
            
            printf("color %u, alpha %u at %u %u \n", color, alpha, x, y);
        }
    }
}

void read_cimage_chunk(void) {

    RIF_Pool *pool = librif_pool_new(400 * 1000);

    RIF_CImage *image = librif_cimage_open(filename2, pool);
    if(image != NULL){
        
        while(1){
            bool closed;
            bool success = librif_cimage_read(image, (10 * 1000), &closed);
            
            if(success){
                printf("read %zu of %zu \n", image->readBytes, image->totalBytes);
            }
            
            if(!success || closed){
                break;
            }
        }
        
        int x = 0, y = 0;
        
        uint8_t color, alpha;
        librif_cimage_get_pixel(image, x, y, &color, &alpha);
        
        printf("color %u, alpha %u at %u %u \n", color, alpha, x, y);
    }
}

int main(int argc, const char * argv[]) {
    
    read_image();
    read_cimage();
    
    read_image_chunk();
    read_cimage_chunk();
    
    return 0;
}
