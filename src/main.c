#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

volatile sig_atomic_t interrupted = 0;

void handler (int sig) {
    interrupted = 1;
}

int main (int argc, char **argv) {
    int fb_file;
    struct fb_var_screeninfo info;
    unsigned long pixels;
    unsigned int *fb;
    sigset_t mask;
    struct sigaction usr_action;
    unsigned int row;
    unsigned int col;
    unsigned long pos;

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

    /* Set up the SIGINT handler */
    sigfillset (&mask);
    usr_action.sa_handler = handler;
    usr_action.sa_mask = mask;
    usr_action.sa_flags = 0;
    sigaction (SIGINT, &usr_action, NULL);

    /* Hide cursor */
    write (1, CSI, 2);
    write (1, "?25l", 4);

    /* Draw to the screen */
    for (int c = 0; c < 1000000000; c+=100000) {
        for (row = 0; row < info.yres; row++) {
            for (col = 0; col < info.xres; col++) {
                pos = (row * info.xres) + (row * 10) + col;
                *(fb + pos) = row * col + c;
            }
        }
    }

    /* Close the framebuffer */
    munmap (fb, (pixels + (info.yres * 10)) * 4);
    close (fb_file);

    /* Wait for SIGINT */
    while (!interrupted);

    /* Show cursor */
    write (1, CSI, 2);
    write (1, "?25h", 4);

    return 0;
}
