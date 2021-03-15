#include <stdio.h>
#include <string.h>

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

struct audio_fifo {
    uint8_t *data;
    int start, end;
    int _end;
    int size;
};

static void audio_callback(void *userdata,
                           uint8_t *stream,
                           int len)
{
    struct audio_fifo *afifo;
    int i, start;

    afifo = (struct audio_fifo *) userdata;
    start = afifo->start;

    for (i = 0; i < len; i++) {
        if (start != afifo->end) {
            /* FIFO is not empty. */
            stream[i] = afifo->data[start++];
            if (start == afifo->size)
                start = 0;
        } else {
            stream[i] = 0;
        }
    }

    afifo->start = start;
}

static int run_gigatron(struct gigatron_state *gs)
{
    SDL_Window *win;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioDeviceID audio_dev_id;
    SDL_AudioSpec audio_spec;
    struct audio_fifo afifo;
    uint8_t tmp_buf[65536];
    uint32_t *pixels;
    uint32_t last_vsync;
    int is_running;
    int vga_x, vga_y;

    pixels = NULL;
    win = NULL;
    renderer = NULL;
    texture = NULL;
    audio_dev_id = 0;

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

    /* Open the audio device. */
    afifo.data = tmp_buf;
    afifo.start = afifo.end = afifo._end = 0;
    afifo.size = sizeof(tmp_buf);

    memset(&audio_spec, 0, sizeof(audio_spec));
    audio_spec.freq = 31500;
    audio_spec.format = AUDIO_S8;
    audio_spec.channels = 1;
    audio_spec.samples = 2048;
    audio_spec.callback = audio_callback;
    audio_spec.userdata = &afifo;
    audio_dev_id = SDL_OpenAudioDevice(NULL,
                                       0,
                                       &audio_spec,
                                       NULL,
                                       SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (audio_dev_id == 0) {
        fprintf(stderr, "unable to open audio: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    SDL_StartTextInput();

    gigatron_reset(gs, FALSE);

    is_running = TRUE;
    last_vsync = 0;
    vga_x = 0;
    vga_y = 0;

    SDL_PauseAudioDevice(audio_dev_id, 0);

    while (is_running) {
        SDL_Event event;
        uint8_t diff_out;
        uint64_t max_cycles;

        /* To prevent infinite loops here. */
        diff_out = 0;
        max_cycles = gs->num_cycles + 1000000;
        while (gs->num_cycles < max_cycles) {
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

            /* Update the VGA counters. */
            vga_x += 4;

            /* If HSYNC or VSYNC signals changed. */
            diff_out = gs->reg_out ^ gs->prev_out;
            if (diff_out & 0xC0)
              break;
        }

        /* VSYNC raised. */
        if ((diff_out & 0x80) && (gs->reg_out & 0x80)) {
            const uint8_t *keyboard_state;
            uint32_t vsync_diff;

            vsync_diff = SDL_GetTicks() - last_vsync;
            if (vsync_diff < 17)
                SDL_Delay(17 - vsync_diff);
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
                case SDL_TEXTINPUT:
                    gs->in = event.text.text[0];
                    break;
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        is_running = FALSE;
                    } else {
                        keyboard_state = SDL_GetKeyboardState(NULL);

                        gs->in = 0;
                        /* UP */
                        if (keyboard_state[SDL_SCANCODE_UP])
                            gs->in |= 8;

                        /* DOWN */
                        if (keyboard_state[SDL_SCANCODE_DOWN])
                            gs->in |= 4;

                        /* LEFT */
                        if (keyboard_state[SDL_SCANCODE_LEFT])
                            gs->in |= 2;

                        /* RIGHT */
                        if (keyboard_state[SDL_SCANCODE_RIGHT])
                            gs->in |= 1;

                        /* B */
                        if (keyboard_state[SDL_SCANCODE_END])
                            gs->in |= 64;

                        /* A */
                        if (keyboard_state[SDL_SCANCODE_HOME])
                            gs->in |= 128;

                        /* START */
                        if (keyboard_state[SDL_SCANCODE_PAGEUP])
                            gs->in |= 16;

                        /* SELECT */
                        if (keyboard_state[SDL_SCANCODE_PAGEDOWN])
                            gs->in |= 32;

                        /* Input is negated. */
                        gs->in = 0xFF ^ gs->in;

                        /* Other key codes. */
                        if (event.type == SDL_KEYDOWN) {
                            int fn;

                            if ((event.key.keysym.mod               \
                                 & (KMOD_LCTRL | KMOD_RCTRL)) != 0) {
                                /* For Ctrl-c events. */
                                if (event.key.keysym.sym == SDLK_c)
                                    gs->in = 3;
                            }

                            if (keyboard_state[SDL_SCANCODE_TAB])
                                gs->in = '\t';

                            if (keyboard_state[SDL_SCANCODE_RETURN])
                                gs->in = '\n';

                            if (keyboard_state[SDL_SCANCODE_BACKSPACE])
                                gs->in = 127;

                            if (keyboard_state[SDL_SCANCODE_DELETE])
                                gs->in = 127;

                            for (fn = 1; fn <= 12; fn++) {
                                if (keyboard_state[SDL_SCANCODE_F1 - fn + 1])
                                    gs->in = 0xC0 + fn;
                            }
                        }
                    }
                    break;
                }
            }
        }

        /* VSYNC falling edge. */
        if ((diff_out & 0x80) && !(gs->reg_out & 0x80)) {
            vga_y = -36;
            /* vga_x = 0; */

            SDL_LockAudioDevice(audio_dev_id);
            afifo.end = afifo._end;
            SDL_UnlockAudioDevice(audio_dev_id);
        }

        /* HSYNC raised. */
        if ((diff_out & 0x40) && (gs->reg_out & 0x40)) {
            int next_end;

            vga_x = -44;
            vga_y++;

            next_end = afifo._end + 1;
            if (next_end == afifo.size)
                next_end = 0;

            if (next_end != afifo.start) {
                afifo.data[afifo._end] = gs->reg_acc;
                afifo._end = next_end;
            }
        }
    }

    SDL_StopTextInput();

    SDL_CloseAudioDevice(audio_dev_id);
    free(pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return TRUE;

fail_run:
    if (audio_dev_id != 0) SDL_CloseAudioDevice(audio_dev_id);
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

    if (!run_gigatron(&gs)) {
        gigatron_destroy(&gs);
        return 1;
    }

    gigatron_destroy(&gs);
    return 0;
}
