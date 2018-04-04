#include <curses.h>
#include <fcntl.h>
#include <png.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#define ERROR(MSG) write (2, MSG, strlen (MSG))

const char *ALLOC_ERR_MSG = "Unable to allocate memory!\n";
const char *OPEN_ERR_MSG = "Unable to open file!\n";
const char *READ_ERR_MSG = "Unable to read from file!\n";
const char *NOT_PNG_MSG = "Not a PNG file!\n";
const char *PNG_PTR_MSG = "Error creating PNG pointer!\n";
const char *INFO_PTR_MSG = "Error creating info pointer!\n";
const char *LIBPNG_ERR_MSG = "Libpng error!\n";

unsigned int ll;
unsigned int bpp;
unsigned int *fb;

unsigned int position (uint32_t row, uint32_t col) {
    return (row * (ll / (bpp / 8))) + col;
}

int main (int argc, char **argv) {
    int ret = 0;
    int fb_file;
    int cx;
    unsigned int *fb_buf;
    struct fb_var_screeninfo info;
    struct fb_fix_screeninfo finfo;
    uint32_t row;
    uint32_t col;
    FILE *pic_file;
    WINDOW *main_win;
    uint8_t png_sig [8];
    ssize_t nread;
    png_structp png_ptr = 0;
    png_infop info_ptr = 0;
    png_infop end_info = 0;
    png_bytep *row_pointers;
    png_byte color_type;
    png_byte bit_depth;
    uint32_t height;
    uint32_t width;

    /* Test arguments */
    if (argc != 2) {
        printf ("Usage: %s dir\n", *argv);
        printf ("   dir - path where the slideshow is\n");
        printf ("         0.png, 1.png, ...\n");
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

    /* Attempt to mmap the framebuffer */
    fb = mmap (0, finfo.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    fb_buf = calloc (finfo.smem_len, 1);
    if ((long)fb == (long)MAP_FAILED || !fb_buf) {
        printf ("Error mapping framebuffer to memory.\n");
        close (fb_file);
        return -1;
    }

    /* Get the line length */
    ll = finfo.line_length;

    /* Get the bits per pixel */
    bpp = info.bits_per_pixel;

    /* Open an image file */
    pic_file = fopen ("../0.png", "rb");
    if (!pic_file) {
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

    /* Read the image */
    nread = fread (png_sig, 1, 8, pic_file);
    if (png_sig_cmp (png_sig, 0, nread)) {
        ERROR(NOT_PNG_MSG);
        ret = -1;
        goto close_ncurses;
    }
    png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if (!png_ptr) {
        ERROR(PNG_PTR_MSG);
        ret = -1;
        goto close_ncurses;
    }
    info_ptr = png_create_info_struct (png_ptr);
    if (!info_ptr) {
        ERROR(INFO_PTR_MSG);
        ret = -1;
        goto close_png;
    }
    end_info = png_create_info_struct (png_ptr);
    if (!info_ptr) {
        ERROR(INFO_PTR_MSG);
        ret = -1;
        goto close_png;
    }

    /* If libong errors, it comes here */
    if (setjmp (png_jmpbuf (png_ptr))) {
        ERROR(LIBPNG_ERR_MSG);
        goto close_png;
    }

    /* Initialize IO */
    png_init_io (png_ptr, pic_file);

    /* Tell libpng about the signature bytes read */
    png_set_sig_bytes (png_ptr, nread);

    /* Read PNG info */
    png_read_info (png_ptr, info_ptr);

    /* Get the image dimensions */
    height = png_get_image_height (png_ptr, info_ptr);
    width = png_get_image_width (png_ptr, info_ptr);

    color_type = png_get_color_type (png_ptr, info_ptr);
    bit_depth = png_get_bit_depth (png_ptr, info_ptr);

    /* png_set_interlace_handling (png_ptr); */
    png_read_update_info (png_ptr, info_ptr);
    bpp = info.bits_per_pixel;

    /* Initialize the rows */
    row_pointers = malloc (height * sizeof (png_bytep));
    for (row = 0; row < height; row++) {
        *(row_pointers + row) = malloc (png_get_rowbytes (png_ptr, info_ptr));
    }

    /* Read the png */
    png_read_image (png_ptr, row_pointers);

    /* Draw to the screen */
    for (row = 0; row < height && row < info.yres; row++) {
        cx = 0;
        for (col = 0; col < width && col < info.xres; col++) {
            /* R */
            *(fb_buf + position (row, col)) |= (*(*(row_pointers + row) + (cx + 0)) << 16) & 0xFF0000;
            /* G */
            *(fb_buf + position (row, col)) |= (*(*(row_pointers + row) + (cx + 1)) << 8) & 0xFF00;
            /* B */
            *(fb_buf + position (row, col)) |= *(*(row_pointers + row) + (cx + 2)) & 0xFF;
            cx += 3;
        }
    }
    memcpy (fb, fb_buf, finfo.smem_len);

close_png:
    png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
close_ncurses:
    while (getch () != 'q');

    /* Close ncurses */
    curs_set (1);
    echo ();
    endwin ();

    printf ("widht: %d\n", width);
    printf ("height: %d\n", height);
    printf ("color type (%d), RGB (%d), RGBA (%d)\n", color_type, PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGBA);
    printf ("bit depth: %d\n", bit_depth);

    /* Close the image file */
    fclose (pic_file);

    /* Close the framebuffer */
    free (fb_buf);
    munmap (fb, finfo.smem_len);
    close (fb_file);

    return ret;
}
