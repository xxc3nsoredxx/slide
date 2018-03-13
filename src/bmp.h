#ifndef BMP_H_20180228_171650
#define BMP_H_20180228_171650
#include <string.h>

#define HEADER_SIZE 14

#define ERR(MSG) write (2, MSG, strlen (MSG))

extern const char *ALLOC_ERR_MSG;
extern const char *OPEN_ERR_MSG;
extern const char *READ_ERR_MSG;

struct pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct image {
    uint32_t width;
    uint32_t height;
    uint16_t bpp;
    struct pixel *data;
};

int read_bmp (int, struct image *);

#endif
