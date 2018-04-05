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
    int slide_count;
    int fb_file;
    int cx;
    unsigned int *fb_buf;
    struct fb_var_screeninfo info;
    struct fb_fix_screeninfo finfo;
    uint32_t row;
    uint32_t col;
    char *fname;
    FILE **pic_file;
    WINDOW *main_win;
    uint8_t png_sig [8];
    ssize_t nread;
    png_structp *png_ptr;
    png_infop *info_ptr;
    png_infop *end_info;
    png_bytep **row_pointers;
    /*
    png_byte color_type;
    png_byte bit_depth;
    */
    uint32_t *height;
    uint32_t *width;
    int cx2;
    int quit = 0;

    /* Test arguments */
    if (argc != 3) {
        printf ("Usage: %s dir count\n", *argv);
        printf ("   dir     path where the slideshow is\n");
        printf ("           0.png, 1.png, ...\n");
        printf ("   count   number of slides\n");
        return -1;
    }

    slide_count = atoi (*(argv + 2));

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

    /* Start ncurses */
    main_win = initscr ();
    raw ();
    noecho ();
    keypad (main_win, TRUE);
    curs_set (0);
    move (0,0);
    refresh ();

    /* Open image files */
    fname = malloc (1001);
    pic_file = malloc (slide_count * sizeof (FILE *));
    png_ptr = malloc (slide_count * sizeof (png_structp));
    info_ptr = malloc (slide_count * sizeof (png_infop));
    end_info = malloc (slide_count * sizeof (png_infop));
    row_pointers = malloc (slide_count * sizeof (png_bytep *));
    width = malloc (slide_count * sizeof (uint32_t));
    height = malloc (slide_count * sizeof (uint32_t));
    for (cx = 0; cx < slide_count; cx++) {
        memset (fname, 0, 1001);
        sprintf (fname, "%s%d.png", *(argv + 1), cx);
        *(fname + 1000) = 0;
        *(pic_file + cx) = fopen (fname, "rb");
        if (!*(pic_file + cx)) {
            ERROR(OPEN_ERR_MSG);
            return -1;
        }

        /* Read the image */
        nread = fread (png_sig, 1, 8, *(pic_file + cx));
        if (png_sig_cmp (png_sig, 0, nread)) {
            ERROR(NOT_PNG_MSG);
            ret = -1;
            goto close_ncurses;
        }
        *(png_ptr + cx) = png_create_read_struct (PNG_LIBPNG_VER_STRING, 0, 0, 0);
        if (!*(png_ptr + cx)) {
            ERROR(PNG_PTR_MSG);
            ret = -1;
            goto close_ncurses;
        }
        *(info_ptr + cx) = png_create_info_struct (*(png_ptr + cx));
        if (!*(info_ptr + cx)) {
            ERROR(INFO_PTR_MSG);
            ret = -1;
            goto close_png;
        }
        *(end_info + cx) = png_create_info_struct (*(png_ptr + cx));
        if (!*(end_info + cx)) {
            ERROR(INFO_PTR_MSG);
            ret = -1;
            goto close_png;
        }

        /* If libong errors, it comes here */
        if (setjmp (png_jmpbuf (*(png_ptr + cx)))) {
            ERROR(LIBPNG_ERR_MSG);
            goto close_png;
        }

        /* Initialize IO */
        png_init_io (*(png_ptr + cx), *(pic_file + cx));

        /* Tell libpng about the signature bytes read */
        png_set_sig_bytes (*(png_ptr + cx), nread);

        /* Read PNG info */
        png_read_info (*(png_ptr + cx), *(info_ptr + cx));

        /* Get the image dimensions */
        *(height + cx) = png_get_image_height (*(png_ptr + cx), *(info_ptr + cx));
        *(width + cx) = png_get_image_width (*(png_ptr + cx), *(info_ptr + cx));

        /*
        color_type = png_get_color_type (*(png_ptr + cx), *(info_ptr + cx));
        bit_depth = png_get_bit_depth (*(png_ptr + cx), *(info_ptr + cx));
        */

        png_read_update_info (*(png_ptr + cx), *(info_ptr + cx));

        /* Initialize the rows */
        *(row_pointers + cx) = malloc (*(height + cx) * sizeof (png_bytep));
        for (row = 0; row < *(height + cx); row++) {
            *(*(row_pointers + cx) + row) = malloc (png_get_rowbytes (*(png_ptr + cx), *(info_ptr + cx)));
        }

        /* Read the png */
        png_read_image (*(png_ptr + cx), *(row_pointers + cx));
    }

    /* Set slide to initial frame */
    cx = 0;

    do {
        /* Draw to the screen */
        memset (fb_buf, 0, finfo.smem_len);
        for (row = 0; row < *(height + cx ) && row < info.yres; row++) {
            cx2 = 0;
            for (col = 0; col < *(width + cx) && col < info.xres; col++) {
                /* R */
                *(fb_buf + position (row, col)) |= (*(*(*(row_pointers + cx) + row) + (cx2 + 0)) << 16) & 0xFF0000;
                /* G */
                *(fb_buf + position (row, col)) |= (*(*(*(row_pointers + cx) + row) + (cx2 + 1)) << 8) & 0xFF00;
                /* B */
                *(fb_buf + position (row, col)) |= *(*(*(row_pointers + cx) + row) + (cx2 + 2)) & 0xFF;
                cx2 += 3;
            }
        }
        memcpy (fb, fb_buf, finfo.smem_len);

        /* Get input */
        switch (getch ()) {
        case 'q':
        case 'Q':
            quit = 1;
            break;
        case 'h':
        case 'j':
            cx = (cx > 0) ? cx - 1 : cx;
            break;
        case 'H':
        case 'J':
            cx = 0;
            break;
        case 'k':
        case 'l':
            cx = (cx + 1 < slide_count) ? cx + 1 : cx;
            break;
        case 'K':
        case 'L':
            cx = slide_count - 1;
            break;
        default:
            break;
        }
    } while (!quit);

close_png:
    for (cx = 0; cx < slide_count; cx++) {
        png_destroy_read_struct ((png_ptr + cx), (info_ptr + cx), (end_info + cx));
    }
close_ncurses:
    /* Close ncurses */
    curs_set (1);
    echo ();
    endwin ();

    printf ("slides: %d\n", slide_count);
    /*
    printf ("widht: %d\n", width);
    printf ("height: %d\n", height);
    printf ("color type (%d), RGB (%d), RGBA (%d)\n", color_type, PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGBA);
    printf ("bit depth: %d\n", bit_depth);
    */

    /* Close the image file */
    for (cx = 0; cx < slide_count; cx++) {
        fclose (*(pic_file + cx));
    }
    free (pic_file);
    free (fname);
    free (width);
    free (height);

    /* Close the framebuffer */
    free (fb_buf);
    memset (fb, 0, finfo.smem_len);
    munmap (fb, finfo.smem_len);
    close (fb_file);

    return ret;
}
