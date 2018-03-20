#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "bmp.h"

unsigned int ll;
unsigned int bpp;
unsigned int *fb;

unsigned int color (struct pixel p) {
    unsigned int ret = 0;
    ret |= p.r << 16;
    ret |= p.g << 8;
    ret |= p.b;
    return ret;
}

unsigned int position (uint32_t row, uint32_t col) {
    return (row * (ll / (bpp / 8))) + col;
}

void draw (uint32_t row, uint32_t col, struct pixel p) {
    *(fb + position (row, col)) = color (p);
}

int main (int argc, char **argv) {
    int fb_file;
    struct fb_var_screeninfo info;
    struct fb_fix_screeninfo finfo;
    uint32_t row;
    uint32_t col;
    int pic_file;
    WINDOW *main_win;
    struct image *pic;
    int read_success;

    /* Test arguments */
    if (argc != 2) {
        printf ("Usage: %s dir\n", *argv);
        printf ("   dir - path where the slideshow is\n");
        printf ("         0.bmp, 1.bmp, ...\n");
        return -1;
    }

    /* Attempt to open the framebuffer */
    fb_file = open ("/dev/fb0", O_RDWR);
    if (fb_file == -1) {
        printf ("Error opening framebuffer.\n");
        return -1;
    }

    /* Attempt to get information about the screen */
    if (ioctl (fb_file, FBIOGET_VSCREENINFO, &info)) {
        printf ("Error getting variable screen info.\n");
        close (fb_file);
        return -1;
    }
    if (ioctl (fb_file, FBIOGET_FSCREENINFO, &finfo)) {
        printf ("Error getting fixed screen info.\n");
        close (fb_file);
        return -1;
    }

    /* Get the line length */
    ll = finfo.line_length;

    /* Get the bits per pixel */
    bpp = info.bits_per_pixel;

    /*
    printf ("x: %d\n", info.xres);
    printf ("y: %d\n", info.yres);
    */

    /* Attempt to mmap the framebuffer */
    fb = mmap (0, finfo.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    if ((long)fb == (long)MAP_FAILED) {
        printf ("Error mapping framebuffer to memory.\n");
        close (fb_file);
        return -1;
    }

    /* Open an image file */
    pic_file = open ("../0.bmp", O_RDONLY);
    if (pic_file < 0) {
        ERROR(OPEN_ERR_MSG);
        return -1;
    }

    /* Start ncurses */
    main_win = initscr ();
    raw ();
    noecho ();
    keypad (main_win, TRUE);
    curs_set (0);
    move (0,0);
    refresh ();

    pic = malloc (sizeof (struct image));
    read_success = read_bmp (pic_file, pic);

    /*
    if (read_success) {
        for (row = 0; row < pic->height; row++) {
            for (col = 0; col < pic->width; col++) {
                printf ("%02X%02X%02X ",
                        (pic->data + (row * pic->width) + col)->r,
                        (pic->data + (row * pic->width) + col)->g,
                        (pic->data + (row * pic->width) + col)->b);
            }
            printf ("\n");
        }
    }
    */

    /* Draw to the screen */
    if (read_success) {
        for (row = 0; row < pic->height; row++) {
            for (col = 0; col < pic->width; col++) {
                draw (row, col, *(pic->data + (row * pic->width) + col));
            }
        }
    }

    while (getch () != 'q');

    /* Close ncurses */
    curs_set (1);
    endwin ();

    /* Close the image file */
    close (pic_file);

    /* Close the framebuffer */
    munmap (fb, finfo.smem_len);
    close (fb_file);

    return 0;
}
