// Module implementing the Gigatron TTL RAM
`default_nettype none

module gigatron_ram(i_clock,
                    i_raddr,
                    i_waddr,
                    i_we,
                    i_data,
                    o_data);
   parameter RAM_SIZE = 65536;      // RAM size in bytes

   // Input and output wires
   input  wire        i_clock;      // Input clock
   input  wire [15:0] i_raddr;      // Read address
   input  wire [15:0] i_waddr;      // Write address
   input  wire        i_we;         // Write enable signal
   input  wire [7:0]  i_data;       // Input data
   output wire [7:0]  o_data;       // Output data

   // RAM
   reg [7:0]          ram [0:RAM_SIZE-1];

   reg [7:0]          data;         // Current output data
   reg [7:0]          prev_idata;   // Previous input data
   reg                pass_through; // Pass-through

   assign o_data = (pass_through) ? prev_idata : data;

   always @(posedge i_clock)
     begin
        if (i_we)
          ram[i_waddr] <= i_data;
        data <= ram[i_raddr];

        prev_idata <= i_data;
        pass_through <= ((i_raddr == i_waddr) & i_we);
     end

   initial data = 0;
   initial prev_idata = 0;
   initial pass_through = 0;

`ifdef DEBUG
   integer i;
   initial
     begin
        for (i = 0; i < RAM_SIZE; i++)
          ram[i] = 0;
     end
`endif

endmodule
