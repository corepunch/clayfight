#include "support/support.h"
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "stb_image.h"
uint8_t *image_load_rgba(const char *path,int *w,int *h,char *error,size_t capacity) {
    int channels;uint8_t *pixels=stbi_load(path,w,h,&channels,4);
    if(!pixels && error && capacity)snprintf(error,capacity,"%s: %s",path,stbi_failure_reason());
    return pixels;
}
