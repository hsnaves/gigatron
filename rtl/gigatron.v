// Module implementing the Gigatron TTL computer.
`default_nettype none

module gigatron(i_clock,
                i_reset,
                i_in,
                o_out,
                o_xout);
   parameter RAM_SIZE = 36768;      // RAM size in bytes
   parameter ROM_SIZE = 131072;     // ROM size in bytes
   parameter ROM_FILE = "rom.b";    // file with contents of the ROM

   // Compute the size of the ROM in 16-bit words
   localparam ROM_WORD_SIZE = ROM_SIZE >> 1;

   // Input and output wires
   input  wire        i_clock;      // input clock
   input  wire        i_reset;      // reset signal
   input  wire [7:0]  i_in;         // input (controller)
   output wire [7:0]  o_out;        // regular output (video)
   output wire [7:0]  o_xout;       // extended output (audio, LEDs)

   // Registers
   reg [7:0]          reg_acc;      // accumulator
   reg [7:0]          reg_x;        // X index register
   reg [7:0]          reg_y;        // Y index register
   reg [7:0]          reg_pcl;      // Program counter (low)
   reg [7:0]          reg_pch;      // Program counter (high)
   reg [7:0]          reg_ir;       // Instruction register
   reg [7:0]          reg_d;        // Data register
   reg [7:0]          reg_out;      // Output register
   reg [7:0]          reg_xout;     // Extended output register
   reg [7:0]          reg_in;       // Input register

   // RAM
   reg [7:0]          ram [0:RAM_SIZE-1];

   // ROM
   reg [15:0]         rom [0:ROM_WORD_SIZE-1];

   // Buses
   wire [7:0]         bus_data;     // Main bus (data)
   wire [7:0]         bus_alu;      // Result bus (from ALU)

   // Control and status signals
   wire               write_pcl;
   wire               write_pch;
   wire               write_acc;
   wire               write_x;
   wire               write_y;
   wire               write_out;
   wire               write_ram;
   wire               incr_x;
   wire               sel_addrl;
   wire               sel_addrh;
   wire               sel_oper_a;
   wire [2:0]         sel_oper_b;
   wire [1:0]         sel_bus;
   wire               carry_in;
   wire               carry_out;

   // ======================
   // Instruction Fetch Unit
   // ======================
   wire               incr_pch;     // To increment the PC high
   wire [15:0]        pc;           // 16-bit view of the PC register
   wire [15:0]        rom_data;     // The output data from the ROM

   assign incr_pch = (write_pcl)
                     ? (bus_data == {8{1'b1}})
                     : (reg_pcl == {8{1'b1}});
   assign pc = { reg_pch, reg_pcl };
   assign rom_data = rom[pc];

   // Logic for the program counter
   always @(posedge i_clock)
     begin
        if (i_reset)
          begin
             // Set the program counter to zero address
             reg_pch <= 8'b0;
             reg_pcl <= 8'b0;
          end
        else
          begin
             if (write_pcl)
               reg_pcl <= bus_data;
             else
               reg_pcl <= reg_pcl + 8'b1;

             if (write_pch)
               reg_pch <= reg_y;
             else if (incr_pch)
               reg_pch <= reg_pch + 8'b1;
          end
     end

   // Logic for the instruction register
   always @(posedge i_clock)
     reg_ir <= rom_data[7:0];

   // Logic for the data register
   always @(posedge i_clock)
     reg_d <= rom_data[15:8];

   // ============
   // Control Unit
   // ============
   wire [2:0]         opc_insn;     // Opcode instruction
   wire [2:0]         opc_mode;     // Opcode mode
   wire [1:0]         opc_bus;      // Opcode bus
   wire               is_jump;      // Is Jump instruction?
   wire               is_store;     // Is Store instruction?
   wire               is_long_jump; // Is Long jump instruction?
   wire [1:0]         cond;         // For branch condition
   wire               satisfied;    // Branch condition satisfied
   wire               use_x;        // For the RAM address
   wire               use_y;        // For the RAM address

   assign opc_insn = reg_ir[7:5];
   assign opc_mode = reg_ir[4:2];
   assign opc_bus = reg_ir[1:0];

   assign is_jump = (opc_insn == 3'b111);
   assign is_store = (opc_insn == 3'b111);
   assign is_long_jump = (opc_mode == 3'b000);

   assign cond = { carry_out, reg_acc[7] };
   assign satisfied = (cond == 2'b00) ? opc_mode[0] :
                      (cond == 2'b01) ? opc_mode[1] :
                      (cond == 2'b10) ? opc_mode[2]
                                      : 1'b0;

   assign use_x = (opc_mode == 3'b001)
                | (opc_mode == 3'b011)
                | (opc_mode == 3'b111);
   assign use_y = (opc_mode == 3'b010)
                | (opc_mode == 3'b011)
                | (opc_mode == 3'b111);


   assign write_pcl = is_jump & (satisfied | is_long_jump);
   assign write_acc = (~is_jump) & (~is_store) & (opc_mode[2] == 1'b0);
   assign write_x = (~is_jump) & (opc_mode == 3'b100);
   assign write_y = (~is_jump) & (opc_mode == 3'b101);
   assign write_out = (~is_jump) & (~is_store) & (opc_mode[2:1] == 2'b11);
   assign write_pch = is_jump & is_long_jump;
   assign write_ram = is_store;
   assign incr_x = (~is_jump) & (opc_mode == 3'b111);
   assign sel_addrl = (~is_jump) & use_x;
   assign sel_addrh = (~is_jump) & use_y;
   assign sel_oper_a = (opc_insn == 3'b100)
                     | (opc_insn == 3'b101)
                     | (opc_insn == 3'b110);
   assign sel_oper_b = opc_insn;
   assign sel_bus = opc_bus;
   assign carry_in = (opc_insn == 3'b101)
                   | (opc_insn == 3'b111);

   assign bus_data = (sel_bus == 2'b00) ? reg_d :
                     (sel_bus == 2'b01) ? ram_data :
                     (sel_bus == 2'b10) ? reg_acc
                                        : reg_in;

   // ===================
   // Memory Address Unit
   // ===================
   wire [7:0]         addrl;        // RAM address (low)
   wire [7:0]         addrh;        // RAM address (high)
   wire [15:0]        addr;         // RAM adddres (16-bit view)
   wire [7:0]         ram_data;     // Output data from RAM

   assign addrl = (sel_addrl) ? reg_x : reg_d;
   assign addrh = (sel_addrh) ? reg_y : 8'b0;
   assign addr = { addrh, addrl };

   assign ram_data = ram[addr];

   always @(posedge i_clock)
     if (write_ram)
       ram[addr] <= bus_data;

   // ===========================
   // ALU (Arithmetic Logic Unit)
   // ===========================
   wire [7:0]         oper_a;       // Operand A
   wire [7:0]         oper_b;       // Operand B

   assign oper_a = (sel_oper_a) ? reg_acc : 8'b0;
   assign oper_b = (sel_oper_b == 3'b000) ? bus_data :
                   (sel_oper_b == 3'b001) ? (reg_acc & bus_data) :
                   (sel_oper_b == 3'b010) ? (reg_acc | bus_data) :
                   (sel_oper_b == 3'b011) ? (reg_acc ^ bus_data) :
                   (sel_oper_b == 3'b100) ? bus_data :
                   (sel_oper_b == 3'b101) ? ~bus_data :
                   (sel_oper_b == 3'b110) ? 8'b0
                                          : ~reg_acc;

   assign { carry_out, bus_alu } = oper_a + oper_b + {8'b0, carry_in };

   // ==============
   // User registers
   // ==============

   // Accumulator
   always @(posedge i_clock)
     if (write_acc)
       reg_acc <= bus_alu;

   // Y register
   always @(posedge i_clock)
     if (write_y)
       reg_y <= bus_alu;

   // X register
   always @(posedge i_clock)
     if (write_x)
       reg_x <= bus_alu;
     else if (incr_x)
       reg_x <= reg_x + 8'b1;

   // Output register
   assign o_out = reg_out;
   always @(posedge i_clock)
     if (write_out)
       reg_out <= bus_alu;

   // ===========
   // Peripherals
   // ===========
   wire               hsync;        // Horizontal sync
   wire               vsync;        // Vertical sync

   assign hsync = reg_out[6];
   assign vsync = reg_out[7];

   // Extended output register
   assign o_xout = reg_xout;
   always @(posedge i_clock)
     if (hsync)
       reg_xout <= reg_acc;

   // Input register
   always @(posedge i_clock)
     if (vsync)
       reg_in <= i_in;

   // Read the contents of the rom
   initial $readmemb(ROM_FILE, rom);

endmodule
