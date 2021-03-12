#include <verilated.h>
#include <Vtop.h>

#if VM_TRACE
#include <verilated_vcd_c.h>
#endif

// Current simulation time (64-bit unsigned)
vluint64_t main_time = 0;

// Called by $time in Verilog
double sc_time_stamp()
{
  return main_time; // Note does conversion to real, to match SystemC
}

int main(int argc, char** argv, char** env)
{
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
  top->i_reset = 0;
  top->i_in = 0;

  // Simulate until cpu is halted
  while (!Verilated::gotFinish()) {
    main_time++;  // Time passes...

    // Toggle clocks and such
    top->i_clock = !top->i_clock;

    // Evaluate model
    top->eval();

#if VM_TRACE
    // Dump trace data for this cycle
    if (tfp) tfp->dump(main_time);
#endif
  }

  // Final model cleanup
  top->final();

  // Close trace if opened
#if VM_TRACE
  if (tfp) { tfp->close(); tfp = NULL; }
#endif

  //  Coverage analysis (since test passed)
#if VM_COVERAGE
  Verilated::mkdir("logs");
  VerilatedCov::write("logs/coverage.dat");
#endif

  // Destroy model
  delete top; top = NULL;

  return 0;
}
