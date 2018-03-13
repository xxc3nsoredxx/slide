#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "bmp.h"

int main (int argc, char **argv) {
    int fb_file;
    struct fb_var_screeninfo info;
    unsigned long pixels;
    unsigned int *fb;
    uint32_t row;
    uint32_t col;
    unsigned long pos;
    int picture_file;
    struct image *picture;
    int read;

    const char *CSI = "\x1B[";

    /* Attempt to open the framebuffer */
    fb_file = open ("/dev/fb0", O_RDWR);
    if (fb_file == -1) {
        printf ("Error opening framebuffer.\n");
        return -1;
    }

    /* Attempt to get information about the screen */
    if (ioctl (fb_file, FBIOGET_VSCREENINFO, &info)) {
        printf ("Error getting screen info.\n");
        close (fb_file);
        return -1;
    }

    /* Calculate the pixel count */
    pixels = info.xres * info.yres;

    printf ("x: %d\n", info.xres);
    printf ("y: %d\n", info.yres);

    /* Attempt to mmap the framebuffer */
    fb = mmap (0, (pixels + (info.yres * 10)) * 4, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    if ((long)fb == (long)MAP_FAILED) {
        printf ("Error mapping framebuffer to memory.\n");
        close (fb_file);
        return -1;
    }

    /* Hide cursor */
    write (1, CSI, 2);
    write (1, "?25l", 4);

    /* Open an image file */
    picture_file = open ("../0.bmp", O_RDONLY);
    if (picture_file < 0) {
        ERR(OPEN_ERR_MSG);
        return -1;
    }

    picture = malloc (sizeof (struct image));
    read = read_bmp (picture_file, picture);

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

    /* Draw to the screen */
    /*
    for (int c = 0; c < 1000000000; c+=100000) {
        for (row = 0; row < info.yres; row++) {
            for (col = 0; col < info.xres; col++) {
                pos = (row * info.xres) + (row * 10) + col;
                *(fb + pos) = row * col + c;
            }
        }
    }
    */

    /* Close the image file */
    close (picture_file);

    /* Close the framebuffer */
    munmap (fb, (pixels + (info.yres * 10)) * 4);
    close (fb_file);

    /* Show cursor */
    write (1, CSI, 2);
    write (1, "?25h", 4);

    return 0;
}
