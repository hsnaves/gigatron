// Module implementing the Gigatron TTL RAM
`default_nettype none

module gigatron_ram(i_clock,
                    i_addr,
                    i_we,
                    i_data,
                    o_data);
   parameter RAM_SIZE = 65536;      // RAM size in bytes

   // Input and output wires
   input  wire        i_clock;      // Input clock
   input  wire [15:0] i_addr;       // Address
   input  wire        i_we;         // Write enable signal
   input  wire [7:0]  i_data;       // Input data
   output wire [7:0]  o_data;       // Output data

   // RAM
   reg [7:0]          ram [0:RAM_SIZE-1];

   // Address
   reg [15:0]         addr;

   assign o_data = ram[addr];

   always @(posedge i_clock)
     begin
        if (i_we)
          ram[addr] <= i_data;
        addr <= i_addr;
     end

   initial addr = 0;

`ifdef DEBUG
   integer i;
   initial
     begin
        for (i = 0; i < RAM_SIZE; i++)
          ram[i] = 0;
     end
`endif

endmodule
