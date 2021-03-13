#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#include <verilated.h>
#include <Vtop.h>

#if VM_TRACE
#include <verilated_vcd_c.h>
#endif

#define WIDTH  640
#define HEIGHT 480

#define H_FRONT_PORCH    16
#define H_SYNC_PULSE     96
#define H_BACK_PORCH     48
#define H_LINE           800
#define V_FRONT_PORCH    10
#define V_SYNC_PULSE     2
#define V_BACK_PORCH     33
#define V_LINE           525
#define PIXELS_PER_CLOCK 4

// Current simulation time (64-bit unsigned)
vluint64_t main_time = 0;

// Called by $time in Verilog
double sc_time_stamp()
{
    return main_time; // Note does conversion to real, to match SystemC
}

int main(int argc, char** argv, char** env)
{
    SDL_Window *win;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int32_t *pixels;
    int64_t vsync_disable_time;
    int prev_vsync;
    int prev_hsync;
    int running;

    SDL_Init(SDL_INIT_VIDEO);

    win = SDL_CreateWindow("VGA output",
                           0, 0, WIDTH, HEIGHT, 0);

    renderer = SDL_CreateRenderer(win, -1, 0);
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STATIC,
                                WIDTH, HEIGHT);
    pixels = (int32_t *) malloc(WIDTH * HEIGHT * sizeof(int32_t));

    // Set debug level, 0 is off, 9 is highest presently used
    // May be overridden by commandArgs
    Verilated::debug(0);

    // Randomization reset policy
    // May be overridden by commandArgs
    Verilated::randReset(2);

    // Pass arguments so Verilated code can see them
    // This needs to be called before you create any model
    Verilated::commandArgs(argc, argv);

    // Construct the Verilated model, from Vtop.h
    // generated from Verilating "top.v"
    Vtop* top = new Vtop();

#if VM_TRACE
    // If verilator was invoked with --trace argument,
    // and if at run time passed the +trace argument, turn on tracing
    VerilatedVcdC* tfp = NULL;
    const char* flag = Verilated::commandArgsPlusMatch("trace");

    if (flag && 0 == strcmp(flag, "+trace")) {
        Verilated::traceEverOn(true);  // Verilator must compute traced signals
        VL_PRINTF("Enabling waves into logs/vlt_dump.vcd...\n");
        tfp = new VerilatedVcdC;
        top->trace(tfp, 99);  // Trace 99 levels of hierarchy
        Verilated::mkdir("logs");
        tfp->open("logs/vlt_dump.vcd");  // Open the dump file
    }
#endif

    // Set some inputs
    top->i_clock  = 1;
    top->i_reset = 1;
    top->i_in = 0;

    running = 1;
    prev_vsync = 0;
    prev_hsync = 0;
    vsync_disable_time = -1;

    // Simulate until user stops simulation
    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_KEYDOWN:
                switch(event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                }
                break;
            }
        }

        main_time++;  // Time passes...

        // Toggle clocks and such
        top->i_clock = !top->i_clock;
        if (main_time > 2)
            top->i_reset = 0;

        prev_vsync = top->o_vsync;
        prev_hsync = top->o_hsync;

        // Evaluate model
        top->eval();

        if ((!top->o_vsync) && (prev_vsync)) {
            SDL_UpdateTexture(texture, NULL, pixels,
                              WIDTH * sizeof(int32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);

            vsync_disable_time = main_time;
        }

        if ((vsync_disable_time >= 0) && (top->i_clock)) {
            vluint64_t pxl_tdiff;

            pxl_tdiff = (main_time - vsync_disable_time) >> 1;
            pxl_tdiff *= PIXELS_PER_CLOCK;

            if ((pxl_tdiff >= V_BACK_PORCH * H_LINE)
                && (pxl_tdiff < (V_BACK_PORCH + HEIGHT) * H_LINE)) {
                int line, column;

                line = (int) ((pxl_tdiff - V_BACK_PORCH * H_LINE) / H_LINE);
                column = (int) (pxl_tdiff % H_LINE);

                column -= (H_BACK_PORCH + H_SYNC_PULSE + H_FRONT_PORCH);
                if ((column >= 0) && (column < WIDTH)) {
                    Uint32 color;
                    color = (top->o_red) << 16
                          | (top->o_green) << 8
                          | (top->o_blue);

                    pixels[line * WIDTH + column] = color;
                    pixels[line * WIDTH + column + 1] = color;
                    pixels[line * WIDTH + column + 2] = color;
                    pixels[line * WIDTH + column + 3] = color;
                }
            }
        }

        if (Verilated::gotFinish())
            running = 0;

#if VM_TRACE
        // Dump trace data for this cycle
        if (tfp) tfp->dump(main_time);
#endif
    }

    // Final model cleanup
    top->final();

#if VM_TRACE
    // Close trace if opened
    if (tfp) { tfp->close(); tfp = NULL; }
#endif

#if VM_COVERAGE
    //  Coverage analysis (since test passed)
    Verilated::mkdir("logs");
    VerilatedCov::write("logs/coverage.dat");
#endif

    // Destroy model
    delete top; top = NULL;

    free(pixels);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
