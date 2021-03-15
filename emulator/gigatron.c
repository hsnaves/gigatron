/* Gigatron TTL emulator based on the original `gtemu.c` file
 * by Marcel van Kervinck and Walter Belgers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gigatron.h"

/* Auxiliary function for disassemble_gigatron().
 * This prints the memory address referenced by the opcode.
 */
static int print_address(uint8_t opc, uint8_t imm,
                         char *outbuf, size_t size)
{
    uint32_t mod;
    uint32_t d;
    int ret;

    ret = 0;
    mod = (((uint32_t)opc) >> 2) & 0x07;
    d = (uint32_t) imm;

    switch(mod) {
    case 0: case 4: case 5: case 6:
        ret = snprintf(outbuf, size, "[$%02X]", d);
        break;
    case 1:
        ret = snprintf(outbuf, size, "[x]");
        break;
    case 2:
        ret = snprintf(outbuf, size, "[y,$%02X]", d);
        break;
    case 3:
        ret = snprintf(outbuf, size, "[y,x]");
        break;
    case 7:
        ret = snprintf(outbuf, size, "[y,x++]");
        break;
    }
    return ret;
}

/* Auxiliary function for disassemble_gigatron().
 * This prints the bus register/memory referenced by the opcode.
 */
static int print_bus(uint8_t opc, uint8_t imm,
                     char *outbuf, size_t size)
{
    uint32_t ins;
    uint32_t bus;
    uint32_t d;
    int is_write;
    int ret;

    ins = (((uint32_t) opc) >> 5) & 0x07;
    bus = ((uint32_t) opc) & 0x03;
    d = (uint32_t) imm;
    is_write = (ins == 6);

    switch(bus) {
    case 0:
        ret = snprintf(outbuf, size, "$%02X", d);
        break;
    case 1:
        if (!is_write) {
            ret = print_address(opc, imm, outbuf, size);
        } else {
            ret = snprintf(outbuf, size, "??");
        }
        break;
    case 2:
        ret = snprintf(outbuf, size, "acc");
        break;
    case 3:
        ret = snprintf(outbuf, size, "in");
        break;
    }

    return ret;
}

/* Names of the (non-branching) instructions. */
static const char *ins_name[] = {
    "ld", "anda", "ora", "xora", "adda", "suba", "st"
};

/* Names of the branching instructions. */
static const char *branch_name[] = {
    "jmp", "bgt", "blt", "bne", "beq", "bge", "ble", "bra"
};

/* Auxiliary function to manager buffer sizes.
 * This is used in `disassemble_gigatron()`.
 */
static void update_buffer_len(int ret, int *len, size_t *cur_size)
{
    *len += ret;
    if ((*cur_size) >= ((size_t) ret))
        *cur_size -= (size_t) ret;
}

int disassemble_gigatron(uint16_t pc, uint8_t opc, uint8_t imm,
                         char *outbuf, size_t size)
{
    uint32_t ins;
    uint32_t mod;
    uint32_t d;
    int is_write;
    int is_jump;
    int len, ret;
    size_t cur_size;

    ins = (((uint32_t) opc) >> 5) & 7;
    mod = (((uint32_t) opc) >> 2) & 7;
    d = (uint32_t) imm;

    is_write = (ins == 6);
    is_jump = (ins == 7);

    len = 0;
    cur_size = size;

    ret = snprintf(&outbuf[len], cur_size,
                   "%04X: %02X %02X    ",
                   pc, (uint32_t) opc, d);
    if (ret < 0) return ret;
    update_buffer_len(ret, &len, &cur_size);

    if (!is_jump) {
        ret = snprintf(&outbuf[len], cur_size,
                       "%-6s ", ins_name[ins]);
        if (ret < 0) return ret;
        update_buffer_len(ret, &len, &cur_size);

        ret = print_bus(opc, imm,
                        &outbuf[len], cur_size);
        if (ret < 0) return ret;
        update_buffer_len(ret, &len, &cur_size);

        if (is_write) {
            ret = snprintf(&outbuf[len], cur_size,
                           ", ");
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);

            ret = print_address(opc, imm,
                                &outbuf[len], cur_size);
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);
        }

        switch(mod) {
        case 0: case 1: case 2:  case 3:
            break;
        case 4:
            ret = snprintf(&outbuf[len], cur_size,
                           ", x");
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);
            break;
        case 5:
            ret = snprintf(&outbuf[len], cur_size,
                           ", y");
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);
            break;
        case 6:
        case 7:
            ret = snprintf(&outbuf[len], cur_size,
                           ", out");
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);
            break;
        }
    } else {
        ret = snprintf(&outbuf[len], cur_size,
                       "%-6s ", branch_name[mod]);
        if (ret < 0) return ret;
        update_buffer_len(ret, &len, &cur_size);

        if (mod != 0) {
            ret = print_bus(opc, imm,
                            &outbuf[len], cur_size);
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);
        } else {
            ret = snprintf(&outbuf[len], cur_size,
                           "y, ");
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);

            ret = print_bus(opc, imm,
                            &outbuf[len], cur_size);
            if (ret < 0) return ret;
            update_buffer_len(ret, &len, &cur_size);
        }
    }

    return len;
}

int gigatron_create(struct gigatron_state *gs,
                    const char *rom_filename,
                    uint32_t ram_size)
{
    FILE *fp;
    size_t size;

    gs->rom = NULL;
    gs->ram = NULL;

    gs->ram_size = ram_size;
    gs->rom = malloc(65536 * sizeof(uint16_t));
    gs->ram = malloc(ram_size * sizeof(uint8_t));

    if (!gs->rom || !gs->ram) {
        fprintf(stderr, "memory exhausted\n");
        goto fail_create;
    }

    fp = fopen(rom_filename, "r");
    if (!fp) {
        fprintf(stderr, "could not open file `%s` for reading\n",
                rom_filename);
        goto fail_create;
    }

    size = fread(gs->rom, sizeof(uint16_t), 65536, fp);
    fclose(fp);

    if (size != 65536)
        fprintf(stderr, "invalid rom size");

    return TRUE;

fail_create:
    gigatron_destroy(gs);
    return FALSE;
}

void gigatron_destroy(struct gigatron_state *gs)
{
    if (gs->rom) free(gs->rom);
    if (gs->ram) free(gs->ram);
    gs->rom = NULL;
    gs->ram = NULL;
}

void gigatron_reset(struct gigatron_state *gs, int zero_ram)
{
    gs->pc = 0;

    gs->reg_ir = 0x02; /* nop */
    gs->reg_d = 0x00;

    gs->reg_acc = 0;
    gs->reg_x = 0;
    gs->reg_y = 0;
    gs->reg_out = 0;
    gs->reg_xout = 0;
    gs->reg_in = 0;

    gs->prev_pc = 0;
    gs->prev_out = 0;

    if (zero_ram)
        memset(gs->ram, 0, gs->ram_size);

    gs->num_cycles = 0;
}

void gigatron_step(struct gigatron_state *gs)
{
    uint32_t ins;
    uint32_t mod;
    uint32_t bus;
    int is_write;
    int is_jump;
    int increment_x;
    uint8_t low, high, *to;
    uint8_t b, alu;
    uint16_t addr;

    ins = (((uint32_t) gs->reg_ir) >> 5) & 0x07;
    mod = (((uint32_t) gs->reg_ir) >> 2) & 0x07;
    bus = ((uint32_t) gs->reg_ir) & 0x03;
    is_write = (ins == 6);
    is_jump = (ins == 7);

    low = gs->reg_d;
    high = 0;
    to = NULL;
    increment_x = FALSE;

    if (!is_jump) {
        /* Resolve the memory address, and the target register. */
        switch (mod) {
        case 0:
            to = (is_write) ? NULL : &gs->reg_acc;
            break;
        case 1:
            to = (is_write) ? NULL : &gs->reg_acc;
            low = gs->reg_x;
            break;
        case 2:
            to = (is_write) ? NULL : &gs->reg_acc;
            high = gs->reg_y;
            break;
        case 3:
            to = (is_write) ? NULL : &gs->reg_acc;
            low = gs->reg_x;
            high = gs->reg_y;
            break;
        case 4:
            to = &gs->reg_x;
            break;
        case 5:
            to = &gs->reg_y;
            break;
        case 6:
            to = (is_write) ? NULL : &gs->reg_out;
            break;
        case 7:
            to = (is_write) ? NULL : &gs->reg_out;
            low = gs->reg_x;
            high = gs->reg_y;
            increment_x = TRUE;
            break;
        }
    }

    addr = (high << 8) | low;
    b = 0;
    switch(bus) {
    case 0:
        b = gs->reg_d;
        break;
    case 1:
        if (!is_write) {
            if (((size_t) addr) < gs->ram_size) {
                b = gs->ram[addr];
            }
        }
        break;
    case 2:
        b = gs->reg_acc;
        break;
    case 3:
        b = gs->reg_in;
        break;
    }

    switch(ins) {
    case 0: /* ld */
        alu = b;
        break;
    case 1: /* anda */
        alu = gs->reg_acc & b;
        break;
    case 2: /* ora */
        alu = gs->reg_acc | b;
        break;
    case 3: /* xora */
        alu = gs->reg_acc ^ b;
        break;
    case 4: /* adda */
        alu = gs->reg_acc + b;
        break;
    case 5: /* suba */
        alu = gs->reg_acc - b;
        break;
    case 6: /* st */
        alu = gs->reg_acc;
        break;
    case 7: /* branch */
        alu = -gs->reg_acc;
        break;
    }

    /* Modifications to the state are done here. */

    /* Fetch new instruction. */
    gs->reg_ir = (uint8_t) (gs->rom[gs->pc] & 0xFF);
    gs->reg_d = (uint8_t) ((gs->rom[gs->pc] >> 8) & 0xFF);

    /* Update the program counter. */
    gs->prev_pc = gs->pc;
    if (is_jump) {
        if (mod != 0) {
            int cond = (gs->reg_acc >> 7) + 2 * (gs->reg_acc == 0);
            if (mod & (1 << cond)) {
                gs->pc = (gs->pc & 0xFF00) | b;
            } else {
                gs->pc++;
            }
        } else {
            /* Far jump */
            gs->pc = (gs->reg_y << 8) | b;
        }
    } else {
        gs->pc++;
    }

    /* Write back to memory. */
    if (is_write) {
        if (((size_t) addr) < gs->ram_size) {
            gs->ram[addr] = b;
        }
    }

    /* On /HSYNC rising edge, update extended output register
     * and input register.
     */
    if ((gs->reg_out & 0x40) && !(gs->prev_out & 0x40)) {
        gs->reg_xout = gs->reg_acc;
        gs->reg_in = gs->in;
    }

    /* Update the registers. */
    gs->prev_out = gs->reg_out;
    if (to != NULL) *to = alu;
    if (increment_x) gs->reg_x++;

    gs->num_cycles++;
}

int gigatron_disasm(struct gigatron_state *gs,
                    char *outbuf, size_t size)
{
    return disassemble_gigatron(gs->prev_pc, gs->reg_ir, gs->reg_d,
                                outbuf, size);
}
