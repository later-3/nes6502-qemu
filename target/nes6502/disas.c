/*
 * AVR disassembler
 *
 * Copyright (c) 2019-2020 Richard Henderson <rth@twiddle.net>
 * Copyright (c) 2019-2020 Michael Rolnik <mrolnik@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"

typedef struct {
    disassemble_info *info;
    uint32_t addr;
    uint32_t pc;
    uint8_t len;
    uint8_t bytes[8];
    uint16_t next_word;
    bool next_word_used;
} DisasContext;


/* decoder helper */
static uint32_t decode_insn_load_bytes(DisasContext *ctx, uint32_t insn,
                           int i, int n)
{
    uint32_t addr = ctx->addr;

    g_assert(ctx->len == i);
    g_assert(n <= ARRAY_SIZE(ctx->bytes));

    while (++i <= n) {
        ctx->info->read_memory_func(addr++, &ctx->bytes[i - 1], 1, ctx->info);
        insn |= ctx->bytes[i - 1] << (32 - i * 8);
    }
    ctx->addr = addr;
    ctx->len = n;
    return insn;
}


/* Include the auto-generated decoder.  */
static uint32_t decode_insn_load(DisasContext *ctx);
static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"

#define output(mnemonic, format, ...) \
    (pctx->info->fprintf_func(pctx->info->stream, "%-9s " format, \
                              mnemonic, ##__VA_ARGS__))

int nes6502_print_insn(bfd_vma addr, disassemble_info *dis)
{
    DisasContext ctx;
    uint32_t insn;

    ctx.info = dis;
    ctx.pc = ctx.addr = addr;
    ctx.len = 0;

    insn = decode_insn_load(&ctx);
    if (!decode_insn(&ctx, insn)) {
    }
    return ctx.addr - addr;
}


#define INSN(opcode, format, ...)                                       \
static bool trans_##opcode(DisasContext *pctx, arg_##opcode * a)        \
{                                                                       \
    output(#opcode, format, ##__VA_ARGS__);                             \
    return true;                                                        \
}

#define INSN_MNEMONIC(opcode, mnemonic, format, ...)                    \
static bool trans_##opcode(DisasContext *pctx, arg_##opcode * a)        \
{                                                                       \
    output(mnemonic, format, ##__VA_ARGS__);                            \
    return true;                                                        \
}

/*
 * Arithmetic Instructions
 */

INSN(ADC_IM,            "%d", a->imm)
INSN(ADC_ZEROPAGE,      "%d", a->imm)
INSN(ADC_ZEROPAGE_X,    "%d", a->imm)
INSN(ADC_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(ADC_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(ADC_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(ADC_INDIRECT_X,    "%d", a->imm)
INSN(ADC_INDIRECT_Y,    "%d", a->imm)

INSN(SBC_IM,            "%d", a->imm)
INSN(SBC_ZEROPAGE,      "%d", a->imm)
INSN(SBC_ZEROPAGE_X,    "%d", a->imm)
INSN(SBC_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(SBC_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(SBC_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(SBC_INDIRECT_X,    "%d", a->imm)
INSN(SBC_INDIRECT_Y,    "%d", a->imm)

/*
 * Branch Instructions
 */

INSN(BCC,            "%d", a->imm)
INSN(BCS,            "%d", a->imm)
INSN(BEQ,            "%d", a->imm)
INSN(BMI,            "%d", a->imm)
INSN(BVS,            "%d", a->imm)
INSN(BNE,            "%d", a->imm)
INSN(BPL,            "%d", a->imm)
INSN(BVC,            "%d", a->imm)

INSN(JMP_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(JMP_INDIRECT,      "%d, %d", a->addr1, a->addr2)

INSN(JSR_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

INSN(RTS,    "")
INSN(RTI,    "")


/*
 * Data Transfer Instructions
 */

INSN(LDA_IM,            "%d", a->imm)
INSN(LDA_ZEROPAGE,      "%d", a->imm)
INSN(LDA_ZEROPAGE_X,    "%d", a->imm)
INSN(LDA_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(LDA_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(LDA_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(LDA_INDIRECT_X,    "%d", a->imm)
INSN(LDA_INDIRECT_Y,    "%d", a->imm)

INSN(LDX_IM,            "%d", a->imm)
INSN(LDX_ZEROPAGE,      "%d", a->imm)
INSN(LDX_ZEROPAGE_Y,    "%d", a->imm)
INSN(LDX_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(LDX_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)

INSN(LDY_IM,            "%d", a->imm)
INSN(LDY_ZEROPAGE,      "%d", a->imm)
INSN(LDY_ZEROPAGE_X,    "%d", a->imm)
INSN(LDY_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(LDY_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(STA_ZEROPAGE,      "%d", a->imm)
INSN(STA_ZEROPAGE_X,    "%d", a->imm)
INSN(STA_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(STA_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(STA_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(STA_INDIRECT_X,    "%d", a->imm)
INSN(STA_INDIRECT_Y,    "%d", a->imm)

INSN(STX_ZEROPAGE,      "%d", a->imm)
INSN(STX_ZEROPAGE_Y,    "%d", a->imm)
INSN(STX_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

INSN(STY_ZEROPAGE,      "%d", a->imm)
INSN(STY_ZEROPAGE_X,    "%d", a->imm)
INSN(STY_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

INSN(TAX,    "")
INSN(TAY,    "")
INSN(TSX,    "")
INSN(TXS,    "")
INSN(TXA,    "")
INSN(TYA,    "")


/*
 * Bit and Bit-test Instructions
 */
INSN(AND_IM,            "%d", a->imm)
INSN(AND_ZEROPAGE,      "%d", a->imm)
INSN(AND_ZEROPAGE_X,    "%d", a->imm)
INSN(AND_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(AND_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(AND_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(AND_INDIRECT_X,    "%d", a->imm)
INSN(AND_INDIRECT_Y,    "%d", a->imm)

INSN(EOR_IM,            "%d", a->imm)
INSN(EOR_ZEROPAGE,      "%d", a->imm)
INSN(EOR_ZEROPAGE_X,    "%d", a->imm)
INSN(EOR_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(EOR_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(EOR_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(EOR_INDIRECT_X,    "%d", a->imm)
INSN(EOR_INDIRECT_Y,    "%d", a->imm)

INSN(ORA_IM,            "%d", a->imm)
INSN(ORA_ZEROPAGE,      "%d", a->imm)
INSN(ORA_ZEROPAGE_X,    "%d", a->imm)
INSN(ORA_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(ORA_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(ORA_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(ORA_INDIRECT_X,    "%d", a->imm)
INSN(ORA_INDIRECT_Y,    "%d", a->imm)

INSN(ASL_A,             "")
INSN(ASL_ZEROPAGE,      "%d", a->imm)
INSN(ASL_ZEROPAGE_X,    "%d", a->imm)
INSN(ASL_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(ASL_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(LSR_A,             "")
INSN(LSR_ZEROPAGE,      "%d", a->imm)
INSN(LSR_ZEROPAGE_X,    "%d", a->imm)
INSN(LSR_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(LSR_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(ROL_A,             "")
INSN(ROL_ZEROPAGE,      "%d", a->imm)
INSN(ROL_ZEROPAGE_X,    "%d", a->imm)
INSN(ROL_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(ROL_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(ROR_A,             "")
INSN(ROR_ZEROPAGE,      "%d", a->imm)
INSN(ROR_ZEROPAGE_X,    "%d", a->imm)
INSN(ROR_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(ROR_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(BIT_ZEROPAGE,      "%d", a->imm)
INSN(BIT_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

INSN(CLC,    "")
INSN(CLD,    "")
INSN(CLI,    "")
INSN(CLV,    "")
INSN(SEC,    "")
INSN(SED,    "")
INSN(SEI,    "")



/*
 * Comparison
 */

INSN(CMP_IM,            "%d", a->imm)
INSN(CMP_ZEROPAGE,      "%d", a->imm)
INSN(CMP_ZEROPAGE_X,    "%d", a->imm)
INSN(CMP_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(CMP_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)
INSN(CMP_ABSOLUTE_Y,    "%d, %d", a->addr1, a->addr2)
INSN(CMP_INDIRECT_X,    "%d", a->imm)
INSN(CMP_INDIRECT_Y,    "%d", a->imm)

INSN(CPX_IM,            "%d", a->imm)
INSN(CPX_ZEROPAGE,      "%d", a->imm)
INSN(CPX_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)

INSN(CPY_IM,            "%d", a->imm)
INSN(CPY_ZEROPAGE,      "%d", a->imm)
INSN(CPY_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)


/*
 * Increment
 */

INSN(INC_ZEROPAGE,      "%d", a->imm)
INSN(INC_ZEROPAGE_X,    "%d", a->imm)
INSN(INC_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(INC_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(INX,    "")
INSN(INY,    "")



/*
 * Decrement
 */

INSN(DEC_ZEROPAGE,      "%d", a->imm)
INSN(DEC_ZEROPAGE_X,    "%d", a->imm)
INSN(DEC_ABSOLUTE,      "%d, %d", a->addr1, a->addr2)
INSN(DEC_ABSOLUTE_X,    "%d, %d", a->addr1, a->addr2)

INSN(DEX,    "")
INSN(DEY,    "")

/*
 * Stack
 */
INSN(PHP,    "")
INSN(PHA,    "")
INSN(PLA,    "")
INSN(PLP,    "")

/*
 * MCU Control Instructions
 */
INSN(NOP,    "")
INSN(BRK,    "")

