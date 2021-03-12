// Test module for the Gigatron TTL computer.
`default_nettype none

module top(i_clock,
           i_reset,
           i_in,
           o_hsync,
           o_vsync,
           o_red,
           o_green,
           o_blue,
           o_led,
           o_dac);

   input  wire        i_clock;
   input  wire        i_reset;
   input  wire [7:0]  i_in;
   output wire        o_hsync;
   output wire        o_vsync;
   output wire [7:0]  o_red;
   output wire [7:0]  o_green;
   output wire [7:0]  o_blue;
   output wire [3:0]  o_led;
   output wire [3:0]  o_dac;

   wire [7:0]         out;
   wire [7:0]         xout;

   gigatron
     #(.ROM_FILE("../../data/rom.b"))
     g1(.i_clock(i_clock),
        .i_reset(i_reset),
        .i_in(i_in),
        .o_out(out),
        .o_xout(xout));

   assign o_hsync = out[6];
   assign o_vsync = out[7];

   assign o_red = { out[1:0], 6'b0 };
   assign o_green = { out[3:2], 6'b0 };
   assign o_blue = { out[5:4], 6'b0 };

   assign o_led = xout[3:0];
   assign o_dac = xout[7:4];

endmodule
