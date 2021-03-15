#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "gigatron.h"

/* For the SDL window */
#define WIDTH  640
#define HEIGHT 480
#define BORDER 60

/* Rudimentary FIFO implementation for the audio callback. */
struct audio_fifo {
    uint8_t *data;
    int start, end;
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
    uint32_t last_vsync; /* Multiplied by 3. */
    uint32_t frame_count;

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

    /* /VSYNC falling edge. */
    if ((diff_out & 0x80) && (gs->reg_out & 0x80)) {
        emu->vga_y = -28;
        /* emu->vga_x = 0; */
     }

    /* /HSYNC raised. */
    if ((diff_out & 0x40) && (gs->reg_out & 0x40)) {
        emu->vga_x = -48 + 4;
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

    /* /HSYNC raised. */
    if ((diff_out & 0x40) && (gs->reg_out & 0x40)) {
        int next_end;

        SDL_LockAudioDevice(emu->audio_dev_id);

        next_end = afifo->end + 1;
        if (next_end == afifo->size)
            next_end = 0;

        if (next_end != afifo->start) {
            afifo->data[afifo->end] = (gs->reg_xout & 0xF0);
            afifo->end = next_end;
        }

        SDL_UnlockAudioDevice(emu->audio_dev_id);
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

                    if ((event.key.keysym.mod \
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

/* Midpoint Circle Algorithm. */
void draw_cicle(SDL_Renderer *renderer,
                int32_t center_x, int32_t center_y,
                int32_t radius, int fill)
{
    const int32_t diameter = (radius * 2);
    int32_t x = (radius - 1);
    int32_t y = 0;
    int32_t tx = 1;
    int32_t ty = 1;
    int32_t error = (tx - diameter);

    while (x >= y) {
        /*  Each of the following renders an octant of the circle. */
        if (fill) {
            SDL_RenderDrawLine(renderer,
                               center_x - x, center_y - y,
                               center_x + x, center_y - y);
            SDL_RenderDrawLine(renderer,
                               center_x - y, center_y - x,
                               center_x + y, center_y - x);
            SDL_RenderDrawLine(renderer,
                               center_x - x, center_y + y,
                               center_x + x, center_y + y);
            SDL_RenderDrawLine(renderer,
                               center_x - y, center_y + x,
                               center_x + y, center_y + x);
        } else {
            SDL_RenderDrawPoint(renderer, center_x + x, center_y - y);
            SDL_RenderDrawPoint(renderer, center_x + x, center_y + y);
            SDL_RenderDrawPoint(renderer, center_x - x, center_y - y);
            SDL_RenderDrawPoint(renderer, center_x - x, center_y + y);
            SDL_RenderDrawPoint(renderer, center_x + y, center_y - x);
            SDL_RenderDrawPoint(renderer, center_x + y, center_y + x);
            SDL_RenderDrawPoint(renderer, center_x - y, center_y - x);
            SDL_RenderDrawPoint(renderer, center_x - y, center_y + x);
        }

        if (error <= 0) {
            y++;
            error += ty;
            ty += 2;
        }

        if (error > 0) {
            x--;
            tx += 2;
            error += (tx - diameter);
        }
    }
}

/* Updates the emulator screen.
 * This function returns TRUE if a VSYNC has occurred.
 */
static int update_screen(struct emulator *emu)
{
    struct gigatron_state *gs;
    SDL_Rect src, dst;
    uint8_t diff_out;

    gs = &emu->gs;

    diff_out = gs->reg_out ^ gs->prev_out;

    /* VSYNC raised. */
    if ((diff_out & 0x80) && (gs->reg_out & 0x80)) {
        uint32_t vsync_diff;
        int bit;

        vsync_diff = 3 * SDL_GetTicks() - emu->last_vsync;
        if (vsync_diff < 50)
            SDL_Delay((50 - vsync_diff) / 3);
        emu->last_vsync = 3 * SDL_GetTicks();
        emu->frame_count++;

        src.x = 0;
        src.y = 0;
        src.w = WIDTH;
        src.h = HEIGHT;

        dst.x = BORDER / 2;
        dst.y = BORDER / 2;
        dst.w = WIDTH;
        dst.h = HEIGHT;
        SDL_UpdateTexture(emu->texture, NULL, emu->pixels,
                          WIDTH * sizeof(uint32_t));

        SDL_SetRenderDrawColor(emu->renderer, 0, 0, 0, 0);
        SDL_RenderClear(emu->renderer);
        SDL_RenderCopy(emu->renderer, emu->texture, &src, &dst);

        /* Draw the LEDs. */
        for (bit = 0; bit < 4; bit++) {
            int fill;

            fill = (gs->reg_xout & (1 << bit)) != 0;

            SDL_SetRenderDrawColor(emu->renderer, 127, 0, 0, 0);
            draw_cicle(emu->renderer,
                       (BORDER / 2) + 10 + 30 * bit,
                       (BORDER / 2) + HEIGHT + 12, 10, TRUE);

            if (fill) {
                SDL_SetRenderDrawColor(emu->renderer, 255, 0, 0, 0);
            } else {
                SDL_SetRenderDrawColor(emu->renderer, 0, 0, 0, 0);
            }

            draw_cicle(emu->renderer,
                       (BORDER / 2) + 10 + 30 * bit,
                       (BORDER / 2) + HEIGHT + 12, 7, TRUE);
        }

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
    gs->in = 0xFF;

    emu->is_running = TRUE;
    emu->last_vsync = 0;
    emu->frame_count = 0;
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
                               WIDTH + BORDER,
                               HEIGHT + BORDER,
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
    emu.afifo.start = emu.afifo.end = 0;
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
