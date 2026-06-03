#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

struct framebuffer {
    int fd;
    struct fb_fix_screeninfo fixed_info;
    struct fb_var_screeninfo variable_info;
    size_t size;
    unsigned char *memory;
};

enum input_kind {
    INPUT_KIND_MICE,
    INPUT_KIND_EVENT
};

struct input_source {
    int fd;
    enum input_kind kind;
};

struct app_state {
    struct framebuffer *fb;
    uint32_t background;
    int last_x;
    int last_y;
    int figure_width;
    int figure_height;
    bool drawn;
};

static volatile sig_atomic_t keep_running = 1;
static struct app_state *active_app = NULL;

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static void handle_signal(int signal_number) {
    (void)signal_number;
    keep_running = 0;
}

static void install_signal_handlers(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
}

static uint32_t scale_channel(uint8_t channel, unsigned int length) {
    uint32_t max_value;

    if (length == 0) {
        return 0;
    }

    max_value = (1U << length) - 1U;
    return (uint32_t) ((channel * max_value) / 255U);
}

static uint32_t pack_color(const struct fb_var_screeninfo *info, uint8_t red, uint8_t green, uint8_t blue) {
    return (scale_channel(red, info->red.length) << info->red.offset)
        | (scale_channel(green, info->green.length) << info->green.offset)
        | (scale_channel(blue, info->blue.length) << info->blue.offset);
}

static struct framebuffer open_framebuffer(const char *device_path) {
    struct framebuffer fb;

    memset(&fb, 0, sizeof(fb));
    fb.fd = open(device_path, O_RDWR);
    if (fb.fd == -1) {
        fail("open framebuffer");
    }

    if (ioctl(fb.fd, FBIOGET_FSCREENINFO, &fb.fixed_info) == -1) {
        close(fb.fd);
        fail("FBIOGET_FSCREENINFO");
    }

    if (ioctl(fb.fd, FBIOGET_VSCREENINFO, &fb.variable_info) == -1) {
        close(fb.fd);
        fail("FBIOGET_VSCREENINFO");
    }

    fb.size = (size_t) fb.fixed_info.line_length * fb.variable_info.yres;
    fb.memory = mmap(NULL, fb.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb.fd, 0);
    if (fb.memory == MAP_FAILED) {
        close(fb.fd);
        fail("mmap framebuffer");
    }

    return fb;
}

static void close_framebuffer(struct framebuffer *fb) {
    if (fb->memory != NULL && fb->memory != MAP_FAILED) {
        munmap(fb->memory, fb->size);
    }

    if (fb->fd >= 0) {
        close(fb->fd);
    }
}

static struct input_source open_input_source(const char *device_path) {
    struct input_source source;

    memset(&source, 0, sizeof(source));
    source.fd = open(device_path, O_RDONLY);
    if (source.fd == -1) {
        fail("open input device");
    }

    if (strstr(device_path, "/event") != NULL) {
        source.kind = INPUT_KIND_EVENT;
    } else {
        source.kind = INPUT_KIND_MICE;
    }

    return source;
}

static void close_input_source(struct input_source *source) {
    if (source->fd >= 0) {
        close(source->fd);
    }
}

static void put_pixel(struct framebuffer *fb, int x, int y, uint32_t color) {
    unsigned int bytes_per_pixel;
    size_t offset;

    if (x < 0 || y < 0 || x >= (int) fb->variable_info.xres || y >= (int) fb->variable_info.yres) {
        return;
    }

    bytes_per_pixel = fb->variable_info.bits_per_pixel / 8U;
    offset = ((size_t) y * fb->fixed_info.line_length) + ((size_t) x * bytes_per_pixel);

    switch (bytes_per_pixel) {
        case 4:
            *(uint32_t *) (fb->memory + offset) = color;
            break;
        case 3:
            fb->memory[offset] = (unsigned char) (color & 0xFFU);
            fb->memory[offset + 1] = (unsigned char) ((color >> 8) & 0xFFU);
            fb->memory[offset + 2] = (unsigned char) ((color >> 16) & 0xFFU);
            break;
        case 2:
            *(uint16_t *) (fb->memory + offset) = (uint16_t) color;
            break;
        default:
            break;
    }
}

static void fill_rect(struct framebuffer *fb, int x, int y, int width, int height, uint32_t color) {
    int row;
    int col;

    for (row = y; row < y + height; row++) {
        for (col = x; col < x + width; col++) {
            put_pixel(fb, col, row, color);
        }
    }
}

static void draw_line(struct framebuffer *fb, int x0, int y0, int x1, int y1, int thickness, uint32_t color) {
    int delta_x;
    int delta_y;
    int step_x;
    int step_y;
    int error;
    int doubled_error;

    delta_x = abs(x1 - x0);
    delta_y = abs(y1 - y0);
    step_x = x0 < x1 ? 1 : -1;
    step_y = y0 < y1 ? 1 : -1;
    error = delta_x - delta_y;

    while (true) {
        fill_rect(
            fb,
            x0 - (thickness / 2),
            y0 - (thickness / 2),
            thickness,
            thickness,
            color
        );

        if (x0 == x1 && y0 == y1) {
            break;
        }

        doubled_error = error * 2;
        if (doubled_error > -delta_y) {
            error -= delta_y;
            x0 += step_x;
        }
        if (doubled_error < delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}

static void draw_circle_outline(struct framebuffer *fb, int center_x, int center_y, int radius, int thickness, uint32_t color) {
    int x;
    int y;
    int decision;

    x = radius;
    y = 0;
    decision = 1 - x;

    while (y <= x) {
        fill_rect(fb, center_x + x - (thickness / 2), center_y + y - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x + y - (thickness / 2), center_y + x - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x - y - (thickness / 2), center_y + x - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x - x - (thickness / 2), center_y + y - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x - x - (thickness / 2), center_y - y - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x - y - (thickness / 2), center_y - x - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x + y - (thickness / 2), center_y - x - (thickness / 2), thickness, thickness, color);
        fill_rect(fb, center_x + x - (thickness / 2), center_y - y - (thickness / 2), thickness, thickness, color);

        y++;
        if (decision <= 0) {
            decision += (2 * y) + 1;
        } else {
            x--;
            decision += (2 * (y - x)) + 1;
        }
    }
}

static void clear_screen(struct framebuffer *fb, uint32_t color) {
    fill_rect(
        fb,
        0,
        0,
        (int) fb->variable_info.xres,
        (int) fb->variable_info.yres,
        color
    );
}

static void draw_stickman(struct framebuffer *fb, int center_x, int center_y, int width, int height, uint32_t color) {
    int head_radius;
    int thickness;
    int top_y;
    int head_center_y;
    int neck_y;
    int hip_y;
    int arm_y;
    int shoulder_left_x;
    int shoulder_right_x;
    int hand_left_x;
    int hand_right_x;
    int foot_left_x;
    int foot_right_x;
    int foot_y;

    thickness = width / 24;
    if (thickness < 2) {
        thickness = 2;
    }

    head_radius = width / 8;
    if (head_radius < 4) {
        head_radius = 4;
    }

    top_y = center_y - (height / 2);
    head_center_y = top_y + head_radius;
    neck_y = top_y + (2 * head_radius);
    hip_y = center_y + (height / 6);
    arm_y = neck_y + ((hip_y - neck_y) / 3);
    shoulder_left_x = center_x - (width / 6);
    shoulder_right_x = center_x + (width / 6);
    hand_left_x = center_x - (width / 3);
    hand_right_x = center_x + (width / 3);
    foot_left_x = center_x - (width / 3);
    foot_right_x = center_x + (width / 3);
    foot_y = center_y + (height / 2);

    draw_circle_outline(fb, center_x, head_center_y, head_radius, thickness, color);
    draw_line(fb, center_x, neck_y, center_x, hip_y, thickness, color);
    draw_line(fb, shoulder_left_x, arm_y, hand_left_x, arm_y + (height / 10), thickness, color);
    draw_line(fb, shoulder_right_x, arm_y, hand_right_x, arm_y + (height / 10), thickness, color);
    draw_line(fb, center_x, hip_y, foot_left_x, foot_y, thickness, color);
    draw_line(fb, center_x, hip_y, foot_right_x, foot_y, thickness, color);
}

static int clamp_value(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

static void redraw_figure(struct app_state *app, int x, int y, uint32_t foreground) {
    if (app->drawn) {
        draw_stickman(
            app->fb,
            app->last_x,
            app->last_y,
            app->figure_width,
            app->figure_height,
            app->background
        );
    }

    draw_stickman(app->fb, x, y, app->figure_width, app->figure_height, foreground);
    app->last_x = x;
    app->last_y = y;
    app->drawn = true;
}

static bool read_mouse_delta_mice(int fd, int *delta_x, int *delta_y) {
    unsigned char packet[3];
    ssize_t bytes_read;

    bytes_read = read(fd, packet, sizeof(packet));
    if (bytes_read == -1) {
        if (errno == EINTR) {
            return false;
        }

        fail("read /dev/input/mice");
    }

    if (bytes_read != (ssize_t) sizeof(packet)) {
        return false;
    }

    *delta_x = (int) ((signed char) packet[1]);
    *delta_y = -(int) ((signed char) packet[2]);
    return true;
}

static bool read_mouse_delta_event(int fd, int *delta_x, int *delta_y) {
    struct input_event event;
    ssize_t bytes_read;
    int accumulated_x = 0;
    int accumulated_y = 0;
    bool saw_motion = false;

    while (keep_running) {
        bytes_read = read(fd, &event, sizeof(event));
        if (bytes_read == -1) {
            if (errno == EINTR) {
                return false;
            }

            fail("read /dev/input/eventX");
        }

        if (bytes_read != (ssize_t) sizeof(event)) {
            return false;
        }

        if (event.type == EV_REL) {
            if (event.code == REL_X) {
                accumulated_x += event.value;
                saw_motion = true;
            } else if (event.code == REL_Y) {
                accumulated_y -= event.value;
                saw_motion = true;
            }
        }

        if (event.type == EV_SYN && event.code == SYN_REPORT) {
            if (saw_motion) {
                *delta_x = accumulated_x;
                *delta_y = accumulated_y;
                return true;
            }

            return false;
        }
    }

    return false;
}

static bool read_mouse_delta(const struct input_source *source, int *delta_x, int *delta_y) {
    *delta_x = 0;
    *delta_y = 0;

    if (source->kind == INPUT_KIND_EVENT) {
        return read_mouse_delta_event(source->fd, delta_x, delta_y);
    }

    return read_mouse_delta_mice(source->fd, delta_x, delta_y);
}

int main(int argc, char **argv) {
    struct framebuffer fb;
    struct input_source input;
    struct app_state app;
    uint32_t background;
    uint32_t foreground;
    int figure_width;
    int figure_height;
    int current_x;
    int current_y;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    int delta_x;
    int delta_y;
    const char *input_path;

    fb = open_framebuffer("/dev/fb0");

    if (fb.variable_info.bits_per_pixel != 16 && fb.variable_info.bits_per_pixel != 24 && fb.variable_info.bits_per_pixel != 32) {
        fprintf(stderr, "Bits por pixel no soportados: %u\n", fb.variable_info.bits_per_pixel);
        close_framebuffer(&fb);
        return EXIT_FAILURE;
    }

    background = pack_color(&fb.variable_info, 0, 0, 0);
    foreground = pack_color(&fb.variable_info, 255, 255, 255);

    clear_screen(&fb, background);

    figure_width = (int) fb.variable_info.xres / 24;
    figure_height = (int) fb.variable_info.yres / 9;
    if (figure_width < 18) {
        figure_width = 18;
    }
    if (figure_height < 28) {
        figure_height = 28;
    }

    min_x = figure_width / 2;
    max_x = (int) fb.variable_info.xres - (figure_width / 2) - 1;
    min_y = figure_height / 2;
    max_y = (int) fb.variable_info.yres - (figure_height / 2) - 1;
    current_x = (int) fb.variable_info.xres / 2;
    current_y = (int) fb.variable_info.yres / 2;

    memset(&app, 0, sizeof(app));
    app.fb = &fb;
    app.background = background;
    app.figure_width = figure_width;
    app.figure_height = figure_height;
    active_app = &app;

    install_signal_handlers();

    input_path = argc > 1 ? argv[1] : "/dev/input/mice";
    input = open_input_source(input_path);

    redraw_figure(&app, current_x, current_y, foreground);

    // printf(
    //     "Dibujado en /dev/fb0 (%ux%u, %u bpp). Leyendo mouse desde %s. Usa Ctrl+C para salir.\n",
    //     fb.variable_info.xres,
    //     fb.variable_info.yres,
    //     fb.variable_info.bits_per_pixel,
    //     input_path
    // );
    fflush(stdout);

    while (keep_running) {
        if (!read_mouse_delta(&input, &delta_x, &delta_y)) {
            continue;
        }

        if (delta_x == 0 && delta_y == 0) {
            continue;
        }

        current_x = clamp_value(current_x + delta_x, min_x, max_x);
        current_y = clamp_value(current_y + delta_y, min_y, max_y);
        redraw_figure(&app, current_x, current_y, foreground);
    }

    clear_screen(&fb, background);
    close_input_source(&input);
    close_framebuffer(&fb);
    return EXIT_SUCCESS;
}