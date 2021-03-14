#include <stdio.h>
#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "gigatron.h"

/* For the SDL window */
#define WIDTH  640
#define HEIGHT 480

static void print_help(const char *prog_name)
{
    printf("usage:\n");
    printf("%s [-h | --help] <rom_filename>\n", prog_name);
}

static int run_gigatron(struct gigatron_state *gs)
{
    SDL_Window *win;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t *pixels;
    uint32_t last_vsync;
    int is_running;
    int vga_x, vga_y;
    long long t;

    pixels = NULL;
    win = NULL;
    renderer = NULL;
    texture = NULL;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "unable to initialize SDL: %s\n",
                SDL_GetError());
        return FALSE;
    }

    win = SDL_CreateWindow("Gigatron TTL",
                           SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED,
                           WIDTH,
                           HEIGHT,
                           0);
    if (!win) {
        fprintf(stderr, "unable to create window: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    renderer = SDL_CreateRenderer(win,
                                  -1,
                                  SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "unable to create renderer: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STATIC,
                                WIDTH, HEIGHT);
    if (!texture) {
        fprintf(stderr, "unable to create texture: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    pixels = malloc(WIDTH * HEIGHT * sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "memory exhausted for pixel buffer\n");
        goto fail_run;
    }

    /* One step after COLD reset. */
    gs->pc = 0;
    gigatron_step(gs);
    gs->pc = 0;
    gs->reg_in = 0xFF;

    t = 0;
    is_running = TRUE;
    last_vsync = 0;
    vga_x = 0;
    vga_y = 0;

    while (is_running) {
        SDL_Event event;
        uint8_t prev_out, diff_out;
        long long max_t;

        /* To prevent infinite loops here. */
        diff_out = 0;
        max_t = t + 1000000;
        while (t < max_t) {
            prev_out = gs->reg_out;
            gigatron_step(gs);

            /* Update pixels. */
            if ((vga_x < 640) && (vga_x >= 0)
                && (vga_y >= 0) && (vga_y < 480)) {
                uint32_t color;
                color = ((gs->reg_out & 0x03) << 6) << 16
                      | ((gs->reg_out & 0x0C) << 4) << 8
                      | ((gs->reg_out & 0x30) << 2);

                pixels[vga_y * WIDTH + vga_x] = color;
                pixels[vga_y * WIDTH + vga_x + 1] = color;
                pixels[vga_y * WIDTH + vga_x + 2] = color;
                pixels[vga_y * WIDTH + vga_x + 3] = color;
            }

            /* Update the counters. */
            vga_x += 4;
            t++;

            /* If HSYNC or VSYNC signals changed. */
            diff_out = gs->reg_out ^ prev_out;
            if (diff_out & 0xC0)
              break;
        }

        /* VSYNC raised. */
        if ((diff_out & 0x80) && (gs->reg_out & 0x80)) {
            const uint8_t *keyboard_state;
            uint32_t vsync_diff;

            vsync_diff = SDL_GetTicks() - last_vsync;
            if (vsync_diff < 20)
                SDL_Delay(20 - vsync_diff);
            last_vsync = SDL_GetTicks();

            SDL_UpdateTexture(texture, NULL, pixels,
                              WIDTH * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);

            while (SDL_PollEvent(&event)) {
                switch(event.type) {
                case SDL_QUIT:
                    is_running = FALSE;
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        is_running = FALSE;
                    } else {
                        keyboard_state = SDL_GetKeyboardState(NULL);

                        gs->reg_in = 0;
                        if (keyboard_state[SDL_SCANCODE_UP])
                            gs->reg_in |= 8;

                        if (keyboard_state[SDL_SCANCODE_DOWN])
                            gs->reg_in |= 4;

                        if (keyboard_state[SDL_SCANCODE_LEFT])
                            gs->reg_in |= 2;

                        if (keyboard_state[SDL_SCANCODE_RIGHT])
                            gs->reg_in |= 1;

                        if (keyboard_state[SDL_SCANCODE_Z])
                            gs->reg_in |= 64;

                        if (keyboard_state[SDL_SCANCODE_X])
                            gs->reg_in |= 128;

                        if (keyboard_state[SDL_SCANCODE_RETURN])
                            gs->reg_in |= 16;

                        if (keyboard_state[SDL_SCANCODE_SPACE])
                            gs->reg_in |= 32;

                        gs->reg_in = 0xFF ^ gs->reg_in;
                    }
                    break;
                }
            }
        }

        /* VSYNC falling edge. */
        if ((diff_out & 0x80) && !(gs->reg_out & 0x80)) {
            vga_y = -36;
            /* vga_x = 0; */
        }

        /* HSYNC raised. */
        if ((diff_out & 0x40) && (gs->reg_out & 0x40)) {
            vga_x = -44;
            vga_y++;
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return TRUE;

fail_run:
    if (pixels) free(pixels);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (win) SDL_DestroyWindow(win);

    SDL_Quit();
    return FALSE;
}

int main(int argc, char **argv)
{
    char *rom_filename = "ROM.rom";
    struct gigatron_state gs;
    int i;

    /* Initialize the random number generator. */
    srand(time(NULL));

    for (i = 1; i < argc; i++) {
        if ((strcmp("--help", argv[i]) == 0)
            || (strcmp("-h", argv[i]) == 0)) {

            print_help(argv[0]);
            return 0;
        }

        rom_filename = argv[i];
    }


    if (!gigatron_create(&gs, rom_filename, 65536)) {
        return 1;
    }

    /* The undefined value. */
    gs.undef = (uint8_t) (rand() & 0xFF);

    if (!run_gigatron(&gs)) {
        gigatron_destroy(&gs);
        return 1;
    }

    gigatron_destroy(&gs);
    return 0;
}
