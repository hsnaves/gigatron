# FPGA implementation of the Gigatron TTL microprocessor

## Required software

- [Verilator](https://www.veripool.org/wiki/verilator);
- SDL2 development files;
- Extra utilities: wget, and hexdump.

## Building the code

To build and simulate the computer using [Verilator](https://www.veripool.org/wiki/verilator),
just type:

```shell
make
```

To clean the repository, type
```shell
make clean
```

## Overview of the repository

- The directory `rtl` contains of the Verilog code for the Gigatron TTL cpu;
- The directory `data` contains the ROM file for the Gigatron computer;
- The Verilator code can be found inside `sim`;
