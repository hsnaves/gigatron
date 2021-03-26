// Module implementing the Gigatron TTL computer.
`default_nettype none

module gigatron(i_clock,
                i_reset,
                i_ready,
                i_ram_data,
                i_rom_data,
                i_in,
                o_ram_raddr,
                o_ram_waddr,
                o_ram_data,
                o_ram_we,
                o_rom_addr,
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
   // Input and output wires
   input  wire        i_clock;      // Input clock
   input  wire        i_reset;      // Reset signal
   input  wire        i_ready;      // Ready signal
                                    // (when not asserted, the cpu freezes)
   input  wire [7:0]  i_ram_data;   // Data lines from RAM
   input  wire [15:0] i_rom_data;   // Data lines from ROM
   input  wire [7:0]  i_in;         // Input (controller)
   output wire [15:0] o_ram_raddr;  // RAM read address
   output wire [15:0] o_ram_waddr;  // RAM write address
   output wire [7:0]  o_ram_data;   // RAM input data (for writing)
   output wire        o_ram_we;     // RAM write enable
   output wire [15:0] o_rom_addr;   // ROM address
   output wire [7:0]  o_out;        // Regular output (video)
   output wire [7:0]  o_xout;       // Extended output (audio, LEDs)
`ifdef DEBUG
   output wire [7:0]  o_acc;        // Accumulator output
   output wire [7:0]  o_x;          // X register output
   output wire [7:0]  o_y;          // Y register output
   output wire [7:0]  o_ir;         // Instruction register output
   output wire [7:0]  o_d;          // D register output
   output wire [15:0] o_pc;         // Program counter output
   output wire [7:0]  o_prev_out;   // Previous output
   output wire [15:0] o_prev_pc;    // Previous program counter output
`endif

   // Registers
   reg [7:0]          reg_acc;      // Accumulator
   reg [7:0]          reg_x;        // X index register
   reg [7:0]          reg_y;        // Y index register
   reg [7:0]          reg_pcl;      // Program counter (low)
   reg [7:0]          reg_pch;      // Program counter (high)
   reg [7:0]          reg_ir;       // Instruction register
   reg [7:0]          reg_d;        // Data register
   reg [7:0]          reg_out;      // Output register
   reg [7:0]          reg_prev_out; // Previous output register
   reg [7:0]          reg_xout;     // Extended output register
   reg [7:0]          reg_in;       // Input register

`ifdef DEBUG
   reg [15:0]         reg_prev_pc;  // The previous program pointer
`endif

   // Buses
   wire [7:0]         bus_data;     // Main bus (data)
   wire [7:0]         bus_alu;      // Result bus (from ALU)

   // Control and status signals
   wire               write_pcl;    // Write new value to PC (low)
   wire               write_pch;    // Write new value to PC (high)
   wire               write_acc;    // Write new value to accumulator
   wire               write_x;      // Write new value to X register
   wire               write_y;      // Write new value to Y register
   wire               write_out;    // Write new value to output register
   wire               write_ram;    // Write new value into RAM
   wire               incr_x;       // Increment value of X register
   wire               sel_ram_addrl;// Bit to select RAM address (low)
   wire               sel_ram_addrh;// Bit to select RAM address (high)
   wire               next_sel_ram_addrl;// RAM address low for next cycle
   wire               next_sel_ram_addrh;// RAM address high for next cycle

   // ======================
   // Instruction Fetch Unit
   // ======================
   wire               incr_pch;     // To increment the PC high
   wire [7:0]         next_pcl;     // The next value of the pcl register
   wire [7:0]         next_pch;     // The next value of the pch register
   wire [15:0]        next_pc;      // The next value of the pc register
   wire [7:0]         next_ir;      // The next value of the ir register
   wire [7:0]         next_d;       // The next value of d register

   assign incr_pch = (write_pcl) ? 1'b0 : (&reg_pcl);
   assign next_pc = { next_pch, next_pcl };
   assign o_rom_addr = (i_reset) ? 16'b0 : next_pc;

   // Logic for the program counter
   assign next_pcl = (write_pcl) ? bus_data
                                 : reg_pcl + 8'b1;
   assign next_pch = (write_pch) ? reg_y :
                     (incr_pch)  ? reg_pch + 8'b1
                                 : reg_pch;

   always @(posedge i_clock)
     begin
        if (i_reset)
          begin
             reg_pcl <= 8'b0;
             reg_pch <= 8'b0;
          end
        else if (i_ready)
          begin
             reg_pcl <= next_pcl;
             reg_pch <= next_pch;
          end
     end

`ifdef DEBUG
   // To keep track of the previous program counter
   wire [15:0]        next_prev_pc; // The next value of the previous PC

   assign next_prev_pc = { reg_pch, reg_pcl };

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_prev_pc <= 16'b0;
        else if (i_ready)
          reg_prev_pc <= next_prev_pc;
     end
`endif

   // Logic for the instruction register
   assign next_ir = i_rom_data[7:0];

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_ir <= 8'b0;
        else if (i_ready)
          reg_ir <= next_ir;
     end

   // Logic for the data register
   assign next_d = i_rom_data[15:8];

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_d <= 8'b0;
        else if (i_ready)
          reg_d <= next_d;
     end

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
   wire [3:0]         cond_demux;   // For resolving the branch condition
   wire               satisfied;    // Branch condition satisfied
   wire               use_x;        // For the RAM address
   wire               use_y;        // For the RAM address
   wire [7:0]         masked_ram;   // To avoid simultaneous
                                    // read & write to RAM
   wire [2:0]         next_opc_insn;// The next opcode instruction
   wire [2:0]         next_opc_mode;// The next opcode mode
   wire               next_is_jump; // Is the next instruction a jump?
   wire               next_use_x;   // For the RAM address
   wire               next_use_y;   // For the RAM address

   assign opc_insn = reg_ir[7:5];
   assign opc_mode = reg_ir[4:2];
   assign opc_bus = reg_ir[1:0];

   assign is_jump = (opc_insn == 3'b111);
   assign is_store = (opc_insn == 3'b110);
   assign is_long_jump = (opc_mode == 3'b000);

   assign cond = { ~(|reg_acc), reg_acc[7] };
   assign cond_demux = { 1'b0, opc_mode[2:0] };
   assign satisfied = cond_demux[cond];

   function [7:0] mux4(input [1:0] sel,
                       input [7:0] val0,
                       input [7:0] val1,
                       input [7:0] val2,
                       input [7:0] val3);
      case (sel)
        2'b00: mux4 = val0;
        2'b01: mux4 = val1;
        2'b10: mux4 = val2;
        2'b11: mux4 = val3;
      endcase
   endfunction

   assign masked_ram = (write_ram) ? 8'b0 : i_ram_data;
   assign bus_data = mux4(opc_bus,
                          reg_d,
                          masked_ram,
                          reg_acc,
                          reg_in);

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
   assign sel_ram_addrl = (~is_jump) & use_x;
   assign sel_ram_addrh = (~is_jump) & use_y;

   assign next_opc_insn = next_ir[7:5];
   assign next_opc_mode = next_ir[4:2];
   assign next_is_jump = (next_opc_insn == 3'b111);
   assign next_use_x = (next_opc_mode == 3'b001)
                     | (next_opc_mode == 3'b011)
                     | (next_opc_mode == 3'b111);
   assign next_use_y = (next_opc_mode == 3'b010)
                     | (next_opc_mode == 3'b011)
                     | (next_opc_mode == 3'b111);
   assign next_sel_ram_addrl = (~next_is_jump) & next_use_x;
   assign next_sel_ram_addrh = (~next_is_jump) & next_use_y;

   // ===================
   // Memory Address Unit
   // ===================
   wire [7:0]         ram_raddrl;   // RAM read address (low)
   wire [7:0]         ram_raddrh;   // RAM read address (high)
   wire [7:0]         ram_waddrl;   // RAM write address (low)
   wire [7:0]         ram_waddrh;   // RAM write address (high)

   assign ram_raddrl = (next_sel_ram_addrl) ? next_x : next_d;
   assign ram_raddrh = (next_sel_ram_addrh) ? next_y : 8'b0;
   assign o_ram_raddr = { ram_raddrh, ram_raddrl };
   assign ram_waddrl = (sel_ram_addrl) ? reg_x : reg_d;
   assign ram_waddrh = (sel_ram_addrh) ? reg_y : 8'b0;
   assign o_ram_waddr = { ram_waddrh, ram_waddrl };
   assign o_ram_data = bus_data;
   assign o_ram_we = (write_ram & (~i_reset) & i_ready);

   // ===========================
   // ALU (Arithmetic Logic Unit)
   // ===========================

   function [7:0] alu(input [7:0] oper_a,
                      input [7:0] oper_b,
                      input [2:0] operation);
      case (operation)
        3'b000: alu = oper_b;
        3'b001: alu = oper_a & oper_b;
        3'b010: alu = oper_a | oper_b;
        3'b011: alu = oper_a ^ oper_b;
        3'b100: alu = oper_a + oper_b;
        3'b101: alu = oper_a - oper_b;
        3'b110: alu = oper_a;
        3'b111: alu = 8'b0;
      endcase
   endfunction

   assign bus_alu = alu(reg_acc, bus_data, opc_insn);

   // ==============
   // User registers
   // ==============

   wire [7:0]         next_acc;     // The next value of the accumulator
   wire [7:0]         next_x;       // The next value of the X register
   wire [7:0]         next_y;       // The next value of the Y register
   wire [7:0]         next_out;     // The next value of the output register
   wire [7:0]         next_prev_out;// The next value of the previous output

   // Accumulator
   assign next_acc = (write_acc) ? bus_alu : reg_acc;
   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_acc <= 8'b0;
        else if (i_ready)
          reg_acc <= next_acc;
     end

   // X register
   assign next_x = (write_x)  ? bus_alu :
                   (incr_x)   ? reg_x + 8'b1
                              : reg_x;
   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_x <= 8'b0;
        else if (i_ready)
          reg_x <= next_x;
     end

   // Y register
   assign next_y = (write_y) ? bus_alu : reg_y;

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_y <= 8'b0;
        else if (i_ready)
          reg_y <= next_y;
     end

   // Output register
   assign o_out = reg_out;
   assign next_out = (write_out) ? bus_alu : reg_out;

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_out <= 8'b0;
        else if (i_ready)
          reg_out <= next_out;
     end

   // Previous output register
   assign next_prev_out = reg_out;

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_prev_out <= 8'b0;
        else if (i_ready)
          reg_prev_out <= next_prev_out;
     end

   // ===========
   // Peripherals
   // ===========
   wire               hsync;        // Horizontal sync
   wire               prev_hsync;   // Previous hsync
   wire               hsync_pulse;  // Indicator of a pulse
   wire [7:0]         next_xout;    // The next value of the xout register
   wire [7:0]         next_in;      // The next value of the in register

   assign hsync = reg_out[6];
   assign prev_hsync = reg_prev_out[6];
   assign hsync_pulse = (hsync) & (~prev_hsync);

   // Extended output register
   assign o_xout = reg_xout;
   assign next_xout = (hsync_pulse) ? reg_acc : reg_xout;

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_xout <= 8'b0;
        else if (i_ready)
          reg_xout <= next_xout;
     end

   // Input register
   assign next_in = (hsync_pulse) ? i_in : reg_in;

   always @(posedge i_clock)
     begin
        if (i_reset)
          reg_in <= 8'b0;
        else if (i_ready)
          reg_in <= next_in;
     end

`ifdef DEBUG
   assign o_acc = reg_acc;
   assign o_x = reg_x;
   assign o_y = reg_y;
   assign o_ir = reg_ir;
   assign o_d = reg_d;
   assign o_pc = { reg_pch, reg_pcl };
   assign o_prev_out = reg_prev_out;
   assign o_prev_pc = reg_prev_pc;
`endif

`ifdef FORMAL
   // To verify the operations were performed correctly
   reg                prev_i_reset;

   initial
     begin
        prev_i_reset <= 0;
     end

   always @(posedge i_clock)
     begin
        prev_i_reset <= i_reset;
     end

   always @(*)
     begin
        // Make sure the user registers are zero after reset
        assert(!prev_i_reset || (reg_pcl == 8'b0));
        assert(!prev_i_reset || (reg_pch == 8'b0));
        assert(!prev_i_reset || (reg_acc == 8'b0));
        assert(!prev_i_reset || (reg_x == 8'b0));
        assert(!prev_i_reset || (reg_y == 8'b0));
        assert(!prev_i_reset || (reg_out == 8'b0));
        assert(!prev_i_reset || (reg_xout == 8'b0));
        assert(!prev_i_reset || (reg_in == 8'b0));
     end

`endif // FORMAL

endmodule
