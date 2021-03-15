#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "gigatron.h"

/* For the SDL window */
#define WIDTH  640
#define HEIGHT 480

/* Rudimentary FIFO implementation for the audio callback. */
struct audio_fifo {
    uint8_t *data;
    int start, end;
    int _end;
    int size;
};

/* Callback function to play audio. */
static void audio_callback(void *userdata, uint8_t *stream, int len)
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

/* Structure containing the emulator state and SDL related
 * objects.
 */
struct emulator {
    /* State of the Gigatron TTL computer. */
    struct gigatron_state gs;
    int is_running;

    SDL_Window *win;

    /* Video related fields. */
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint32_t *pixels;
    int vga_x, vga_y;
    uint32_t last_vsync;

    /* Audio related fields. */
    SDL_AudioDeviceID audio_dev_id;
    SDL_AudioSpec audio_spec;
    struct audio_fifo afifo;
    uint8_t abuf[8192];
};

/* Updates the pixels on the frame buffer. */
static void update_pixels(struct emulator *emu)
{
    struct gigatron_state *gs;
    uint8_t diff_out;

    gs = &emu->gs;

    /* Update pixels. */
    if ((emu->vga_x < WIDTH) && (emu->vga_x >= 0)
        && (emu->vga_y >= 0) && (emu->vga_y < HEIGHT)) {
        uint32_t color;
        uint32_t pos;
        color = ((gs->reg_out & 0x03) << 6) << 16
            | ((gs->reg_out & 0x0C) << 4) << 8
            | ((gs->reg_out & 0x30) << 2);

        pos = emu->vga_y * WIDTH + emu->vga_x;
        emu->pixels[pos] = color;
        emu->pixels[pos + 1] = color;
        emu->pixels[pos + 2] = color;
        emu->pixels[pos + 3] = color;
    }

    diff_out = gs->reg_out ^ gs->prev_out;

    /* Update the VGA counters. */
    emu->vga_x += 4;

    /* VSYNC falling edge. */
    if ((diff_out & 0x80) && !(gs->reg_out & 0x80)) {
        emu->vga_y = -36;
        /* emu->vga_x = 0; */
     }

    /* HSYNC raised. */
    if ((diff_out & 0x40) && (gs->reg_out & 0x40)) {
        emu->vga_x = -44;
        emu->vga_y++;
    }
}

/* Adds audio data to the audio FIFO. */
static void update_audio(struct emulator *emu)
{
    struct gigatron_state *gs;
    struct audio_fifo *afifo;
    uint8_t diff_out;

    gs = &emu->gs;
    afifo = &emu->afifo;

    diff_out = gs->reg_out ^ gs->prev_out;

    /* VSYNC falling edge. */
    if ((diff_out & 0x80) && !(gs->reg_out & 0x80)) {
        SDL_LockAudioDevice(emu->audio_dev_id);
        afifo->end = afifo->_end;
        SDL_UnlockAudioDevice(emu->audio_dev_id);
    }

    /* HSYNC raised. */
    if ((diff_out & 0x40) && (gs->reg_out & 0x40)) {
        int next_end;
        next_end = afifo->_end + 1;
        if (next_end == afifo->size)
            next_end = 0;

        if (next_end != afifo->start) {
            afifo->data[afifo->_end] = gs->reg_acc;
            afifo->_end = next_end;
        }
    }
}

/* Process the input events from SDL. */
static void process_input_events(struct emulator *emu)
{
    struct gigatron_state *gs;
    const uint8_t *keyboard_state;
    SDL_Event event;

    gs = &emu->gs;
    while (SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            emu->is_running = FALSE;
            break;
        case SDL_TEXTINPUT:
            gs->in = event.text.text[0];
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                emu->is_running = FALSE;
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

                    if ((event.key.keysym.mod                           \
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

/* Updates the emulator screen.
 * This function returns TRUE if a VSYNC has occurred.
 */
static int update_screen(struct emulator *emu)
{
    struct gigatron_state *gs;
    uint8_t diff_out;

    gs = &emu->gs;

    diff_out = gs->reg_out ^ gs->prev_out;

    /* VSYNC raised. */
    if ((diff_out & 0x80) && (gs->reg_out & 0x80)) {
        uint32_t vsync_diff;

        vsync_diff = SDL_GetTicks() - emu->last_vsync;
        if (vsync_diff < 17)
            SDL_Delay(17 - vsync_diff);
        emu->last_vsync = SDL_GetTicks();

        SDL_UpdateTexture(emu->texture, NULL, emu->pixels,
                          WIDTH * sizeof(uint32_t));
        SDL_RenderClear(emu->renderer);
        SDL_RenderCopy(emu->renderer, emu->texture, NULL, NULL);
        SDL_RenderPresent(emu->renderer);

        return TRUE;
    }

    return FALSE;
}

static void main_loop(struct emulator *emu)
{
    struct gigatron_state *gs;

    gs = &emu->gs;
    gigatron_reset(gs, FALSE);

    emu->is_running = TRUE;
    emu->last_vsync = 0;
    emu->vga_x = 0;
    emu->vga_y = 0;

    while (emu->is_running) {
        uint64_t max_cycles;

        /* To prevent infinite loops here. */
        max_cycles = gs->num_cycles + 1000000;
        while (gs->num_cycles < max_cycles) {
            gigatron_step(gs);

            update_pixels(emu);
            update_audio(emu);
            if (update_screen(emu))
                break;
        }

        process_input_events(emu);
    }
}

static int run_emulator(const char *rom_filename)
{
    struct emulator emu;
    int ret = FALSE;

    emu.win = NULL;
    emu.renderer = NULL;
    emu.texture = NULL;
    emu.pixels = NULL;
    emu.audio_dev_id = 0;

    if (!gigatron_create(&emu.gs, rom_filename, 65536)) {
        return FALSE;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "unable to initialize SDL: %s\n",
                SDL_GetError());
        gigatron_destroy(&emu.gs);
        return FALSE;
    }

    emu.win = SDL_CreateWindow("Gigatron TTL",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               WIDTH,
                               HEIGHT,
                               0);
    if (!emu.win) {
        fprintf(stderr, "unable to create window: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    emu.renderer = SDL_CreateRenderer(emu.win,
                                      -1,
                                      SDL_RENDERER_PRESENTVSYNC);
    if (!emu.renderer) {
        fprintf(stderr, "unable to create renderer: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    emu.texture = SDL_CreateTexture(emu.renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STATIC,
                                    WIDTH, HEIGHT);
    if (!emu.texture) {
        fprintf(stderr, "unable to create texture: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    emu.pixels = malloc(WIDTH * HEIGHT * sizeof(uint32_t));
    if (!emu.pixels) {
        fprintf(stderr, "memory exhausted for pixel buffer\n");
        goto fail_run;
    }

    /* Open the audio device. */
    emu.afifo.data = emu.abuf;
    emu.afifo.start = emu.afifo.end = emu.afifo._end = 0;
    emu.afifo.size = sizeof(emu.abuf);

    memset(&emu.audio_spec, 0, sizeof(emu.audio_spec));
    emu.audio_spec.freq = 31500;
    emu.audio_spec.format = AUDIO_S8;
    emu.audio_spec.channels = 1;
    emu.audio_spec.samples = 2048;
    emu.audio_spec.callback = audio_callback;
    emu.audio_spec.userdata = &emu.afifo;
    emu.audio_dev_id = SDL_OpenAudioDevice(NULL,
                                           0,
                                           &emu.audio_spec,
                                           NULL,
                                           SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (emu.audio_dev_id == 0) {
        fprintf(stderr, "unable to open audio: %s\n",
                SDL_GetError());
        goto fail_run;
    }

    SDL_StartTextInput();

    SDL_PauseAudioDevice(emu.audio_dev_id, 0);

    main_loop(&emu);
    ret = TRUE;

exit_emu:
    SDL_StopTextInput();

    if (emu.audio_dev_id != 0)
        SDL_CloseAudioDevice(emu.audio_dev_id);

    if (emu.pixels)
        free(emu.pixels);

    if (emu.texture)
        SDL_DestroyTexture(emu.texture);

    if (emu.renderer)
        SDL_DestroyRenderer(emu.renderer);

    if (emu.win)
        SDL_DestroyWindow(emu.win);

    SDL_Quit();

    gigatron_destroy(&emu.gs);
    return ret;

fail_run:
    ret = FALSE;
    goto exit_emu;
}


static void print_help(const char *prog_name)
{
    printf("usage:\n");
    printf("%s [-h | --help] <rom_filename>\n", prog_name);
}

int main(int argc, char **argv)
{
    char *rom_filename = "../data/ROMv5a.rom";
    int i;

    for (i = 1; i < argc; i++) {
        if ((strcmp("--help", argv[i]) == 0)
            || (strcmp("-h", argv[i]) == 0)) {

            print_help(argv[0]);
            return 0;
        }

        rom_filename = argv[i];
    }

    if (!run_emulator(rom_filename))
        return 1;

    return 0;
}
