#ifndef BMP_H_20180228_171650
#define BMP_H_20180228_171650

#define HEADER_SIZE 14

#define ERR(MSG) write (2, MSG, strlen (MSG))

const char *ALLOC_ERR_MSG = "Unable to allocate memory!\n";
const char *OPEN_ERR_MSG = "Unable to open file!\n";
const char *READ_ERR_MSG = "Unable to read from file!\n";

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

#endif
