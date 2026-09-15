/* Baseline JFIF encoder for Switch icons: no EXIF/ICC/Photoshop segments, no restart markers. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
int main(int argc, char **argv) {
    int w, h, n;
    unsigned char *px = stbi_load(argv[1], &w, &h, &n, 3);
    if (!px || w != 256 || h != 256) return 1;
    return stbi_write_jpg(argv[2], w, h, 3, px, 90) ? 0 : 2;
}
