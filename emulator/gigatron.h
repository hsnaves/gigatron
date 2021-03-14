
#ifndef __GIGATRON_H
#define __GIGATRON_H

#include <stdint.h>

/* Constants. */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/* Data structures and type declarations. */

/* The state of the computer. */
struct gigatron_state {
    uint16_t pc;         /* Program counter. */
    uint16_t prev_pc;    /* Previous program counter. */
    uint8_t  reg_ir;     /* Instruction register. */
    uint8_t  reg_d;      /* Data register. */
    uint8_t  reg_acc;    /* Accumulator. */
    uint8_t  reg_x;      /* X index register. */
    uint8_t  reg_y;      /* Y index register. */
    uint8_t  reg_out;    /* Output register. */
    uint8_t  reg_in;     /* Input register. */
    uint8_t  undef;      /* Undefined (for "broken" opcodes). */

    uint16_t *rom;       /* Pointer to the beginning of the ROM. */
    uint8_t *ram;        /* Pointer to the beginninf of the RAM. */
    uint32_t ram_size;   /* The size of the RAM (in bytes). */
};

/* Exported functions. */

/* Dissembles the opcode given by the pair `(opc, imm)` at
 * the address `pc`. The output is written in `outbuf`, which is
 * a buffer of size `size`. The return value is the number of
 * characters written (excluding the null byte at the end of
 * strings) in case the buffer is large enough. Otherwise, it
 * returns the number of characters that would have been written
 * (as in snprintf() function). This function never writes more
 * than `size` bytes into `outbuf`, including the null byte at
 * the end of the string.
 */
int gigatron_disassemble(char *outbuf, size_t size,
                         uint16_t pc, uint8_t opc, uint8_t imm);


/* Creates a new instance of a CPU (populated in `gs`).
 * The name of the file with the contents of the ROM is contained
 * in the parameter `rom_filename`, and the size of the RAM is
 * dictated by `ram_size`.
 * On success, this function returns TRUE.
 */
int gigatron_create(struct gigatron_state *gs,
                    const char *rom_filename,
                    uint32_t ram_size);

/* Deallocates the memory allocated by `gigatron_create()`. */
void gigatron_destroy(struct gigatron_state *gs);

/* Executes one instruction.
 * The state `gs` is updated with the new CPU state after executing
 * the instruction.
 */
void gigatron_step(struct gigatron_state *gs);

/* Convenient wrapper around `gigatron_disassemble()` for the
 * gigatron_state `gs`.
 */
int gigatron_disasm(char *outbuf, size_t size,
                    struct gigatron_state *gs);

#endif /* __GIGATRON_H */
