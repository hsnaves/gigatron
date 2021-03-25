// Module implementing the Gigatron TTL ROM
`default_nettype none

module gigatron_rom(i_clock,
                    i_addr,
                    o_data);
   parameter ROM_SIZE = 131072;     // ROM size in bytes
   parameter ROM_FILE = "rom.hex";  // file with contents of the ROM

   // Compute the size of the ROM in 16-bit words
   localparam ROM_WORD_SIZE = ROM_SIZE >> 1;

   // Input and output wires
   input  wire        i_clock;      // Input clock
   input  wire [15:0] i_addr;       // Address
   output wire [15:0] o_data;       // Output data

   // ROM
   reg [15:0]         rom [0:ROM_WORD_SIZE-1];

   // Data
   reg [15:0]         data;

   assign o_data = data;

   always @(posedge i_clock)
     begin
        data <= rom[i_addr];
     end

   initial data = 0;

   // Read the contents of the rom
   initial $readmemh(ROM_FILE, rom);

endmodule
