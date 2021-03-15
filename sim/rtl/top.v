// Test module for the Gigatron TTL computer.
`default_nettype none

module top(i_clock,
           i_reset,
           i_in,
           o_out,
           o_prev_out,
`ifdef DEBUG
           o_xout,
           o_acc,
           o_x,
           o_y,
           o_ir,
           o_d,
           o_pc,
           o_prev_pc
`else
           o_xout
`endif
           );

   input  wire        i_clock;
   input  wire        i_reset;
   input  wire [7:0]  i_in;
   output wire [7:0]  o_out;
   output wire [7:0]  o_prev_out;
   output wire [7:0]  o_xout;
`ifdef DEBUG
   output wire [7:0]  o_acc;
   output wire [7:0]  o_x;
   output wire [7:0]  o_y;
   output wire [7:0]  o_ir;
   output wire [7:0]  o_d;
   output wire [15:0] o_pc;
   output wire [15:0] o_prev_pc;
`endif

   gigatron
     #(.ROM_FILE("../../data/rom.hex"))
     g1(.i_clock(i_clock),
        .i_reset(i_reset),
        .i_in(i_in),
        .o_out(o_out),
        .o_prev_out(o_prev_out),
`ifdef DEBUG
        .o_xout(o_xout),
        .o_acc(o_acc),
        .o_x(o_x),
        .o_y(o_y),
        .o_ir(o_ir),
        .o_d(o_d),
        .o_pc(o_pc),
        .o_prev_pc(o_prev_pc)
`else
        .o_xout(o_xout)
`endif
        );

endmodule
