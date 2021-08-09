#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

const char *FB_ERR_MSG      = "Error opening framebuffer!";
const char *VINFO_ERR_MSG   = "Error getting variable screen info.";
const char *FINFO_ERR_MSG   = "Error getting fixed screen info.";
const char *FB_MAP_ERR_MSG  = "Error mapping framebuffer to memory.";
const char *ALLOC_ERR_MSG   = "Unable to allocate memory!";
const char *OPEN_ERR_MSG    = "Unable to open file!";
const char *READ_ERR_MSG    = "Unable to read from file!";
const char *NOT_PNG_MSG     = "Not a PNG file!";
const char *PNG_PTR_MSG     = "Error creating PNG pointer!";
const char *INFO_PTR_MSG    = "Error creating info pointer!";
const char *LIBPNG_ERR_MSG  = "Libpng error!";

unsigned int ll;
unsigned int bpp;
struct fb_var_screeninfo info;
struct fb_fix_screeninfo finfo;
unsigned int *fb;
unsigned int *fb_buf;

/* Calculates the index into the framebuffer */
unsigned int position (uint32_t row, uint32_t col) {
    return (row * (ll / (bpp / 8))) + col;
}

/* Draws an image to the screen */
void draw (png_bytep *rows, uint32_t width, uint32_t height) {
    unsigned int row;
    unsigned int col;
    unsigned int offset = 0;
    int cx;

    /* Clear the backbuffer */
    memset(fb_buf, 0, finfo.smem_len);

    /* Calculate the centering offset */
    if (width < info.xres)
        offset = (info.xres - width) / 2;
    if (height < info.yres)
        offset += ((info.yres - height) / 2) * (ll / (bpp / 8));

    /* Extract the RGB values and draw put them into the backbuffer */
    for (row = 0; row < height && row < info.yres; row++) {
        cx = 0;
        for (col = 0; col < width && col < info.xres; col++) {
            /* R */
            fb_buf[position(row, col) + offset] |= (rows[row][cx + 0] << 16) & 0xFF0000;
            /* G */
            fb_buf[position(row, col) + offset] |= (rows[row][cx + 1] << 8) & 0xFF00;
            /* B */
            fb_buf[position(row, col) + offset] |= rows[row][cx + 2] & 0xFF;
            cx += 3;
        }
    }
    /* Copy to the screen */
    memcpy(fb, fb_buf, finfo.smem_len);
}

int main (int argc, char **argv) {
    const char *error = 0;
    int ret = 0;
    int slide_count;
    int fb_file;
    int cx;
    unsigned int row;
    char *fname = 0;
    FILE **pic_file = 0;
    uint8_t png_sig [8];
    ssize_t nread;
    png_structp *png_ptr = 0;
    png_infop *info_ptr = 0;
    png_infop *end_info = 0;
    png_color_16 *background = 0;
    png_bytep **row_pointers = 0;
    png_byte bit_depth;
    png_byte color_type;
    png_byte interlace_type;
    uint32_t *width = 0;
    uint32_t *height = 0;

    /* Test arguments */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s dir count\n", *argv);
        fprintf(stderr, "   dir     path where the slideshow is\n");
        fprintf(stderr, "           0.png, 1.png, ...\n");
        fprintf(stderr, "   count   number of slides\n");
        ret = EINVAL;
        goto close;
    }

    slide_count = atoi(argv[2]);

    /* Attempt to open the framebuffer */
    fb_file = open("/dev/fb0", O_RDWR);
    if (fb_file == -1) {
        ret = errno;
        error = FB_ERR_MSG;
        goto close;
    }

    /* Attempt to get information about the screen */
    if (ioctl(fb_file, FBIOGET_VSCREENINFO, &info)) {
        ret = errno;
        error = VINFO_ERR_MSG;
        goto close;
    }
    if (ioctl(fb_file, FBIOGET_FSCREENINFO, &finfo)) {
        ret = errno;
        error = FINFO_ERR_MSG;
        goto close;
    }

    fb_buf = calloc(finfo.smem_len, 1);
    /* Attempt to mmap the framebuffer */
    fb = mmap(0, finfo.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    if ((long)fb == (long)MAP_FAILED || !fb_buf) {
        ret = errno;
        error = FB_MAP_ERR_MSG;
        goto close;
    }

    /* Get the line length */
    ll = finfo.line_length;

    /* Get the bits per pixel */
    bpp = info.bits_per_pixel;

    /* Start ncurses */
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    move(0,0);
    refresh();

    /* Open image files */
    fname = malloc(1001);
    pic_file = calloc(slide_count, sizeof(*pic_file));

    for (cx = 0; cx < slide_count; cx++) {
        memset(fname, 0, 1001);
        sprintf(fname, "%s%d.png", argv[1], cx);
        fname[1000] = 0;
        pic_file[cx] = fopen(fname, "rb");
        if (!pic_file[cx]) {
            ret = errno;
            error = OPEN_ERR_MSG;
            goto close_ncurses;
        }
    }

    /* Read images */
    png_ptr = calloc(slide_count, sizeof(*png_ptr));
    info_ptr = calloc(slide_count, sizeof(*info_ptr));
    end_info = calloc(slide_count, sizeof(*end_info));
    background = calloc(slide_count, sizeof(*background));
    row_pointers = calloc(slide_count, sizeof(*row_pointers));
    width = calloc(slide_count, sizeof(*width));
    height = calloc(slide_count, sizeof(*height));

    for (cx = 0; cx < slide_count; cx++) {
        nread = fread(png_sig, 1, 8, pic_file[cx]);
        if (png_sig_cmp(png_sig, 0, nread)) {
            error = NOT_PNG_MSG;
            goto close_ncurses;
        }
        png_ptr[cx] = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
        if (!png_ptr[cx]) {
            error = PNG_PTR_MSG;
            goto close_ncurses;
        }
        info_ptr[cx] = png_create_info_struct(png_ptr[cx]);
        if (!info_ptr[cx]) {
            error = INFO_PTR_MSG;
            goto close_png;
        }
        end_info[cx] = png_create_info_struct(png_ptr[cx]);
        if (!end_info[cx]) {
            error = INFO_PTR_MSG;
            goto close_png;
        }
        background[cx].index = 0;
        background[cx].red = 0;
        background[cx].green = 0;
        background[cx].blue = 0;
        background[cx].gray = 0;

        /* If libpng errors, it comes here */
        if (setjmp(png_jmpbuf(png_ptr[cx]))) {
            error = LIBPNG_ERR_MSG;
            goto close_png;
        }

        /* Initialize IO */
        png_init_io(png_ptr[cx], pic_file[cx]);

        /* Tell libpng about the signature bytes read */
        png_set_sig_bytes(png_ptr[cx], nread);

        /* Read PNG info */
        png_read_info(png_ptr[cx], info_ptr[cx]);

        bit_depth = png_get_bit_depth(png_ptr[cx], info_ptr[cx]);
        color_type = png_get_color_type(png_ptr[cx], info_ptr[cx]);
        interlace_type = png_get_interlace_type(png_ptr[cx], info_ptr[cx]);

        /* Set transformations */
        if (bit_depth == 16)
            png_set_scale_16(png_ptr[cx]);

        if (color_type & PNG_COLOR_MASK_ALPHA)
            png_set_background(png_ptr[cx], (background + cx), PNG_BACKGROUND_GAMMA_SCREEN, 0, 1);

        if (interlace_type != PNG_INTERLACE_NONE)
            png_set_interlace_handling(png_ptr[cx]);

        /* Update info structs */
        png_read_update_info(png_ptr[cx], info_ptr[cx]);

        /* Get the image dimensions */
        height[cx] = png_get_image_height(png_ptr[cx], info_ptr[cx]);
        width[cx] = png_get_image_width(png_ptr[cx], info_ptr[cx]);

        /* Initialize the rows */
        row_pointers[cx] = calloc(height[cx], sizeof(**row_pointers));
        for (row = 0; row < height[cx]; row++) {
            row_pointers[cx][row] = calloc(png_get_rowbytes(png_ptr[cx], info_ptr[cx]), 1);
        }

        /* Read the png */
        png_read_image(png_ptr[cx], row_pointers[cx]);

        /* Read the end */
        png_read_end(png_ptr[cx], end_info[cx]);
    }

    /* Set slide to initial frame */
    cx = 0;

    while (1) {
        /* Draw slide to the screen */
        draw(row_pointers[cx], width[cx], height[cx]);

        /* Get input */
        switch (getch()) {
        case 'q':
        case 'Q':
            goto close_all;
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
    }

close_all:
close_png:
    for (cx = 0; cx < slide_count; cx++) {
        if (row_pointers[cx]) {
            for (row = 0; row < height[cx]; row++) {
                if (row_pointers[cx][row])
                    free(row_pointers[cx][row]);
            }
            free(row_pointers[cx]);
        }
    }

    for (cx = 0; cx < slide_count; cx++) {
        png_destroy_read_struct((png_ptr + cx), (info_ptr + cx), (end_info + cx));
    }

close_ncurses:
    /* Close ncurses */
    curs_set(1);
    echo();
    endwin();

    printf("slides: %d\n", slide_count);
    if (error) {
        fprintf(stderr, "%s\n", error);
        if (ret)
            fprintf(stderr, "%s\n", strerror(ret));
    }

close:
    if (height)
        free(height);
    if (width)
        free(width);
    if (row_pointers)
        free(row_pointers);
    if (background)
        free(background);
    if (end_info)
        free(end_info);
    if (info_ptr)
        free(info_ptr);
    if (png_ptr)
        free(png_ptr);

    for (cx = 0; cx < slide_count; cx++) {
        if (pic_file[cx])
            fclose(pic_file[cx]);
    }
    if (pic_file)
        free(pic_file);
    if (fname)
        free(fname);

    if (fb) {
        memset(fb, 0, finfo.smem_len);
        munmap(fb, finfo.smem_len);
    }
    if (fb_buf)
        free(fb_buf);
    if (fb_file)
        close(fb_file);

    return ret;
}
