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

void read_bmp (int fd) {
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

    /* Attempt to read the BMP header */
    nread = read (fd, bmp_header, HEADER_SIZE);
    if (nread != HEADER_SIZE) {
        ERR(READ_ERR_MSG);
        return;
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
        ERR(READ_ERR_MSG);
        return;
    }

    /* Reverse the endianness if needed */
    if (reverse_endian) {
        rev (dib_size, 4);
    }

    /* Read the rest of the DIB header */
    dib_header = calloc (*(uint32_t *)dib_size - 4, 1);
    if (!dib_header) {
        ERR(ALLOC_ERR_MSG);
        return;
    }
    nread = read (fd, dib_header, *(uint32_t *)dib_size - 4);
    if (nread != *(uint32_t *)dib_size - 4) {
        ERR(READ_ERR_MSG);
        free (dib_header);
        return;
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

    /* Get the height order
     * + is bottom to top
     * - is top to bottom
     */
    bot_top = *(int32_t *)height > 0;

    free (dib_header);
}

int main () {
    int fd = open ("../0.bmp", O_RDONLY);
    if (fd < 0) {
        write (2, OPEN_ERR_MSG, strlen (OPEN_ERR_MSG));
        return -1;
    }
    read_bmp (fd);
    close (fd);
    return 0;
}
