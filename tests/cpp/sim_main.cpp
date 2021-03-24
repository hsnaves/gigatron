#include <stdlib.h>
#include <string.h>

#include <verilated.h>
#include <Vtop.h>

#if VM_TRACE
#include <verilated_vcd_c.h>
#endif

extern "C" {
#include "gigatron.h"
}

// Current simulation time (64-bit unsigned)
static vluint64_t main_time;

// Called by $time in Verilog
double sc_time_stamp()
{
    return main_time; // Note does conversion to real, to match SystemC
}

static const char *rom_filename = "../../data/ROMv5a.rom";

int main(int argc, char** argv, char** env)
{
    struct gigatron_state gs;
    char buffer[256];
    int running;

    // Set debug level, 0 is off, 9 is highest presently used
    // May be overridden by commandArgs
    Verilated::debug(0);

    // Randomization reset policy
    // May be overridden by commandArgs
    Verilated::randReset(2);

    // Pass arguments so Verilated code can see them
    // This needs to be called before you create any model
    Verilated::commandArgs(argc, argv);

    if (!gigatron_create(&gs, rom_filename, 65536)) {
        return 1;
    }

    gigatron_reset(&gs, TRUE);

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
    top->i_clock = 0;
    top->i_reset = 1;
    top->i_ready = 1;

    // Evaluate model
    top->eval();

    // Evaluate model
    top->i_clock = 1;
    top->eval();

    top->i_reset = 0;
    top->i_in = 0;

    main_time = 0;
    running = TRUE;

    // Simulate
    while (running) {
        main_time++;  // Time passes...

        // Toggle clocks and such
        top->i_clock = !top->i_clock;

        // Evaluate model
        top->eval();

        if (top->i_clock) {
            gigatron_step(&gs);
            if ((gs.pc != top->o_pc)
                || (gs.prev_pc != top->o_prev_pc)
                || (gs.reg_ir != top->o_ir)
                || (gs.reg_d != top->o_d)
                || (gs.reg_acc != top->o_acc)
                || (gs.reg_x != top->o_x)
                || (gs.reg_y != top->o_y)
                || (gs.reg_out != top->o_out)
                || (gs.prev_out != top->o_prev_out)
                || (gs.reg_xout != top->o_xout)) {

                gigatron_disasm(&gs, buffer, sizeof(buffer));
                printf("%s\n\n", buffer);
                printf("            emu   verilog\n");
                printf("%8s: $%04X $%04X\n",
                       "pc", gs.pc, top->o_pc);
                printf("%8s: $%04X $%04X\n",
                       "prev_pc", gs.prev_pc, top->o_prev_pc);
                printf("%8s:   $%02X   $%02X\n",
                       "ir", gs.reg_ir, top->o_ir);
                printf("%8s:   $%02X   $%02X\n",
                       "d", gs.reg_d, top->o_d);
                printf("%8s:   $%02X   $%02X\n",
                       "acc", gs.reg_acc, top->o_acc);
                printf("%8s:   $%02X   $%02X\n",
                       "x", gs.reg_x, top->o_x);
                printf("%8s:   $%02X   $%02X\n",
                       "y", gs.reg_y, top->o_y);
                printf("%8s:   $%02X   $%02X\n",
                       "out", gs.reg_out, top->o_out);
                printf("%8s:   $%02X   $%02X\n",
                       "xout", gs.reg_xout, top->o_xout);
                printf("\n\n");

                running = FALSE;
            }
        }

        if (Verilated::gotFinish()
            || (main_time > 2 * 1000000))
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

    gigatron_destroy(&gs);

    return 0;
}
