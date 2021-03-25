// Test module for the Gigatron TTL computer.
`default_nettype none

module top(i_clock,
           i_reset,
           i_ready,
           i_in,
           o_out,
`ifdef DEBUG
           o_xout,
           o_acc,
           o_x,
           o_y,
           o_ir,
           o_d,
           o_pc,
           o_prev_out,
           o_prev_pc
`else
           o_xout
`endif
           );

   input  wire        i_clock;
   input  wire        i_reset;
   input  wire        i_ready;
   input  wire [7:0]  i_in;
   output wire [7:0]  o_out;
   output wire [7:0]  o_xout;
`ifdef DEBUG
   output wire [7:0]  o_acc;
   output wire [7:0]  o_x;
   output wire [7:0]  o_y;
   output wire [7:0]  o_ir;
   output wire [7:0]  o_d;
   output wire [15:0] o_pc;
   output wire [7:0]  o_prev_out;
   output wire [15:0] o_prev_pc;
`endif

   // RAM and ROM wires
   wire [7:0]         ram_data_in;
   wire [7:0]         ram_data_out;
   wire [15:0]        ram_addr;
   wire               ram_we;
   wire [15:0]        rom_data;
   wire [15:0]        rom_addr;

   gigatron_ram
     gram1(.i_clock(i_clock),
           .i_addr(ram_addr),
           .i_we(ram_we),
           .i_data(ram_data_out),
           .o_data(ram_data_in));

   gigatron_rom
     #(.ROM_FILE("../../data/rom.hex"))
     grom1(.i_clock(i_clock),
           .i_addr(rom_addr),
           .o_data(rom_data));

   gigatron
     g1(.i_clock(i_clock),
        .i_reset(i_reset),
        .i_ready(i_ready),
        .i_ram_data(ram_data_in),
        .i_rom_data(rom_data),
        .i_in(i_in),
        .o_ram_addr(ram_addr),
        .o_ram_data(ram_data_out),
        .o_ram_we(ram_we),
        .o_rom_addr(rom_addr),
        .o_out(o_out),
`ifdef DEBUG
        .o_xout(o_xout),
        .o_acc(o_acc),
        .o_x(o_x),
        .o_y(o_y),
        .o_ir(o_ir),
        .o_d(o_d),
        .o_pc(o_pc),
        .o_prev_out(o_prev_out),
        .o_prev_pc(o_prev_pc)
`else
        .o_xout(o_xout)
`endif
        );

endmodule
