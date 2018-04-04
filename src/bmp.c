#include <curses.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "bmp.h"

/* Checks endianness of a system
 * return 1: same endian
 * return 0: different endian
 */
char check_end (const int fd, const uint32_t size) {
    off_t cur = lseek (fd, 0L, SEEK_CUR);
    off_t s = lseek (fd, 0L, SEEK_END);
    lseek (fd, cur, SEEK_SET);
    return s == (off_t)size;
}

/* Reverses the endianness */
void rev (uint8_t *arr, const unsigned int len) {
    unsigned int cx;
    for (cx = 0; cx < len / 2; cx++) {
        *(arr + cx) ^= *(arr + (len - cx - 1));
        *(arr + (len - cx - 1)) ^= *(arr + cx);
        *(arr + cx) ^= *(arr + (len - cx - 1));
    }
}

/* Future me or anyone that reads this:
 * Sorry for all the ugly typecating that is about to come!
 * 
 * Reads a BMP from file and assignes it to the struct pointed to by picture
 * Return 1 on success, 0 on fail
 */
int read_bmp (int fd, struct image* picture) {
    int ret;
    size_t nread;
    uint8_t reverse_endian;
    uint8_t bmp_header [HEADER_SIZE];
    uint8_t magic [2];
    uint8_t size [4];
    uint8_t data_off [4];
    uint8_t dib_size [4];
    /* The DIB header (minus bytes for size) */
    uint8_t *dib_header;
    uint8_t width [4];
    uint8_t height [4];
    uint8_t c_planes [2];
    uint8_t bpp [2];
    uint8_t compression [4];
    uint8_t data_size [4];
    uint8_t ppm_h [4];
    uint8_t ppm_v [4];
    uint8_t n_palette [4];
    uint8_t n_important [4];
    uint8_t bot_top;
    /* The color palette (if used) */
    unsigned int dib_end;
    unsigned int palette_bytes;
    struct pixel *palette;
    /* The pixel data read from file */
    uint8_t *data;
    /* The actual pixels of the image */
    struct pixel *pixels;
    unsigned int row;
    unsigned int col;
    int pad;
    uint8_t *temp;
    struct pixel temp_pixel;

    /* Attempt to read the BMP header */
    nread = read (fd, bmp_header, HEADER_SIZE);
    if (nread != HEADER_SIZE) {
        ERROR(READ_ERR_MSG);
        goto err;
    }

    /* Get the magic bytes */
    memcpy (magic, bmp_header, 2);

    /* Get the size of the file from the header */
    memcpy (size, (bmp_header + 2), 4);

    /* Get the data offset */
    memcpy (data_off, (bmp_header + 10), 4);

    /* Check endianness */
    reverse_endian = !check_end (fd, *(uint32_t *)size);

    /* Reverse the endianness if needed */
    if (reverse_endian) {
        rev (size, 4);
                rev (data_off, 4);
            }

            /* Read the DIB size */
            nread = read (fd, dib_size, 4);
            if (nread != 4) {
                ERROR(READ_ERR_MSG);
                goto err;
            }

            /* Reverse the endianness if needed */
            if (reverse_endian) {
                rev (dib_size, 4);
            }

            /* Read the rest of the DIB header */
            dib_header = calloc (*(uint32_t *)dib_size - 4, 1);
            if (!dib_header) {
                ERROR(ALLOC_ERR_MSG);
                goto err;
            }
            nread = read (fd, dib_header, *(uint32_t *)dib_size - 4);
            if (nread != *(uint32_t *)dib_size - 4) {
                ERROR(READ_ERR_MSG);
                goto err;
            }

            /* Get the width and height */
            memcpy (width, dib_header, 4);
            memcpy (height, (dib_header + 4), 4);

            /* Get the number of color planes */
            memcpy (c_planes, (dib_header + 8), 2);

            /* Get the bits per pixel */
            memcpy (bpp, (dib_header + 10), 2);

            /* Get the compression method */
            memcpy (compression, (dib_header + 12), 4);

            /* Get the size of the raw data (plus padding) */
            memcpy (data_size, (dib_header + 16), 4);

            /* Get the horizontal and vertical pixels/meter */
            memcpy (ppm_h, (dib_header + 20), 4);
            memcpy (ppm_v, (dib_header + 24), 4);

            /* Get the number of colors in the palette */
            memcpy (n_palette, (dib_header + 28), 4);

            /* Get the number of important colors */
            memcpy (n_important, (dib_header + 32), 4);

            /* Reverse the endianness if needed */
            if (reverse_endian) {
                rev (width, 4);
                rev (height, 4);
                rev (c_planes, 2);
                rev (bpp, 2);
                rev (compression, 4);
                rev (data_size, 4);
                rev (ppm_h, 4);
                rev (ppm_v, 4);
                rev (n_palette, 4);
                rev (n_important, 4);
            }

            printw ("bpp: %d\n", *(uint16_t *)bpp);

            /* Get the height order
             * + is bottom to top
             * - is top to bottom
             */
            bot_top = *(int32_t *)height > 0;

            /* Get the palette information */
            if (*(uint32_t *)n_palette || *(uint16_t *)bpp <= 8) {
                dib_end = 14 + *(uint32_t *)dib_size;
                palette_bytes = *(uint32_t *)data_off - dib_end;
                palette = calloc (palette_bytes / 4, sizeof (struct pixel));
                if (!palette) {
                    ERROR(ALLOC_ERR_MSG);
                    goto err;
                }
                nread = read (fd, palette, palette_bytes);
                if (nread != palette_bytes) {
                    ERROR(READ_ERR_MSG);
                    goto err;
                }
            }

            /* Read the pixel data from the file */
            data = calloc (*(uint32_t *)data_size, 1);
            if (!data) {
                ERROR(ALLOC_ERR_MSG);
                goto err;
            }
            nread = read (fd, data, *(uint32_t *)data_size);
            if (nread != *(uint32_t *)data_size) {
                ERROR(READ_ERR_MSG);
                goto err;
            }

            pixels = calloc (*(uint32_t *)width * *(uint32_t *) height,
                             sizeof (struct pixel));
            if (!pixels) {
                ERROR(ALLOC_ERR_MSG);
                goto err;
            }
            /* Zero bytes padded to the end of each row for 4-byte allignment */
            pad = (*(uint32_t *)width * *(uint16_t *)bpp / 8) % 4;
            /* Don't deal with palette just yet */
            /* --------CHANGE THIS, TEMPORARY ONLY FOR 24bpp!!!!-------- */
            temp = calloc (3, 1);
            if (!temp) {
                ERROR(ALLOC_ERR_MSG);
        goto err;
    }
    for (row = 0; row < *(uint32_t *)height; row++) {
        for (col = 0; col < *(uint32_t *)width; col++) {
            memcpy (temp,
               (data + (row * 3 * *(uint32_t *)width) + (row * pad) + (col * 3)), 3);
            temp_pixel.b = *temp;
            temp_pixel.g = *(temp + 1);
            temp_pixel.r = *(temp + 2);
            (pixels + ((*(uint32_t *)height - row - 1) * *(uint32_t *)width) + col)->r = temp_pixel.r;
            (pixels + ((*(uint32_t *)height - row - 1) * *(uint32_t *)width) + col)->g = temp_pixel.g;
            (pixels + ((*(uint32_t *)height - row - 1) * *(uint32_t *)width) + col)->b = temp_pixel.b;
        }
    }

    picture->width = *(uint32_t *)width;
    picture->height = *(uint32_t *)height;
    picture->bpp = *(uint16_t *)bpp;
    picture->data = pixels;
    /* Set pixels to point to null to ensure that it doesn't get freed early */
    pixels = (void *)0;

    ret = 1;
    goto cleanup;

err:
    ret = 0;
cleanup:
    if (temp)       free (temp);
    if (pixels)     free (pixels);
    if (palette)    free (palette);
    if (dib_header) free (dib_header);

    return ret;
}

/*
int main () {
    int fd = open ("../0.bmp", O_RDONLY);
    struct image *picture = {0};
    int read;
    uint32_t row;
    uint32_t col;
    if (fd < 0) {
        write (2, OPEN_ERR_MSG, strlen (OPEN_ERR_MSG));
        return -1;
    }

    read = read_bmp (fd, picture);

    if (!read) {
        for (row = 0; row < picture->height; row++) {
            for (col = 0; col < picture->width; col++) {
                printf ("%02X%02X%02X ",
                        (picture->data + (row * picture->width) + col)->r,
                        (picture->data + (row * picture->width) + col)->g,
                        (picture->data + (row * picture->width) + col)->b);
            }
            printf ("\n");
        }
    }

    close (fd);
    return 0;
}
*/
