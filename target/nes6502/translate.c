/*
 * QEMU AVR CPU
 *
 * Copyright (c) 2019-2020 Michael Rolnik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/lgpl-2.1.html>
 */

#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "tcg/tcg.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "tcg/tcg-op.h"
#include "exec/cpu_ldst.h"
#include "exec/helper-proto.h"
#include "exec/helper-gen.h"
#include "exec/log.h"
#include "exec/translator.h"
#include "exec/gen-icount.h"
#include "exec/address-spaces.h"
#include "litenes_cpu.h"

/*
 *  Define if you want a BREAK instruction translated to a breakpoint
 *  Active debugging connection is assumed
 *  This is for
 *  https://github.com/seharris/qemu-avr-tests/tree/master/instruction-tests
 *  tests
 */
#undef BREAKPOINT_ON_BREAK

static TCGv cpu_pc;

static TCGv cpu_A;
static TCGv cpu_X;
static TCGv cpu_Y;

static TCGv cpu_Cf;
static TCGv cpu_Zf;
static TCGv cpu_Nf;
static TCGv cpu_Vf;
static TCGv cpu_Sf;
static TCGv cpu_Hf;
static TCGv cpu_Tf;
static TCGv cpu_If;

static TCGv cpu_carry_flag;
static TCGv cpu_zero_flag;
static TCGv cpu_interrupt_flag;
static TCGv cpu_decimal_flag;
static TCGv cpu_break_flag;
static TCGv cpu_unused_flag;
static TCGv cpu_overflow_flag;
static TCGv cpu_negative_flag;
static TCGv cpu_stack_point;
static TCGv P;

static TCGv op_address;
static TCGv op_value;

static TCGv cpu_rampD;
static TCGv cpu_rampX;
static TCGv cpu_rampY;
static TCGv cpu_rampZ;

static TCGv cpu_r[NUMBER_OF_CPU_REGISTERS];
static TCGv cpu_eind;
static TCGv cpu_sp;

static TCGv cpu_skip;

static const char reg_names[NUMBER_OF_CPU_REGISTERS][8] = {
    "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
};
#define REG(x) (cpu_r[x])

#define DISAS_JUMP    DISAS_TARGET_0
#define DISAS_UPDATE  DISAS_TARGET_1
#define DISAS_EXIT    DISAS_TARGET_2

typedef struct DisasContext DisasContext;

/* This is the state at translation time. */
struct DisasContext {
    DisasContextBase base;

    CPUNES6502State *env;
    CPUState *cs;

    target_long npc;
    uint32_t opcode;

    /* Routine used to access memory */
    int memidx;

    TCGv skip_var0;
    TCGv skip_var1;
    TCGCond skip_cond;
};

void nes6502_cpu_tcg_init(void)
{
    int i;

#define NES6502_REG_OFFS(x) offsetof(CPUNES6502State, x)
    cpu_pc = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(pc_w), "pc");
    cpu_Cf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregC), "Cf");
    cpu_Zf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregZ), "Zf");
    cpu_Nf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregN), "Nf");
    cpu_Vf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregV), "Vf");
    cpu_Sf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregS), "Sf");
    cpu_Hf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregH), "Hf");
    cpu_Tf = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregT), "Tf");
    cpu_If = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sregI), "If");
    cpu_rampD = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampD), "rampD");
    cpu_rampX = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampX), "rampX");
    cpu_rampY = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampY), "rampY");
    cpu_rampZ = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(rampZ), "rampZ");
    cpu_eind = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(eind), "eind");
    cpu_sp = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(sp), "sp");
    cpu_skip = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(skip), "skip");

    cpu_A = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(reg_A), "reg_A");
    cpu_X = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(reg_X), "reg_X");
    cpu_Y = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(reg_Y), "reg_Y");
    cpu_carry_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(carry_flag), "carry_flag");
    cpu_zero_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(zero_flag), "zero_flag");
    cpu_interrupt_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(interrupt_flag), "interrupt_flag");
    cpu_decimal_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(decimal_flag), "decimal_flag");
    cpu_break_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(break_flag), "break_flag");
    cpu_unused_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(unused_flag), "unused_flag");
    cpu_overflow_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(overflow_flag), "overflow_flag");
    cpu_negative_flag = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(negative_flag), "negative_flag");
    cpu_stack_point = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(stack_point), "stack_point");
    op_address = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(op_address), "op_address");
    op_value = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(op_value), "op_value");
    P = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(P), "P");

    for (i = 0; i < NUMBER_OF_CPU_REGISTERS; i++) {
        cpu_r[i] = tcg_global_mem_new_i32(cpu_env, NES6502_REG_OFFS(r[i]),
                                          reg_names[i]);
    }
#undef NES6502_REG_OFFS
}

/* decoder helper */
static uint32_t decode_insn_load_bytes(DisasContext *ctx, uint32_t insn,
                           int i, int n)
{
    while (++i <= n) {
        uint8_t b = cpu_ldub_code(ctx->env, ctx->base.pc_next++);
        insn |= b << (32 - i * 8);
    }
    return insn;
}

static uint32_t decode_insn_load(DisasContext *ctx);
static bool decode_insn(DisasContext *ctx, uint32_t insn);
#include "decode-insn.c.inc"

static void nes6502_ld_tl(TCGv_i32 val, TCGv addr, TCGArg idx, MemOp memop)
{
    gen_helper_print_cpu_ram_ld_b(cpu_env, addr);
    tcg_gen_qemu_ld_tl(val, addr, idx, memop);
}

// CPU Addressing Modes
static void cpu_address_zero_page(uint8_t addr)
{
    tcg_gen_movi_tl(op_address, addr);
    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_zero_page_x(uint8_t imm)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_addi_tl(tmp, cpu_X, imm);
    tcg_gen_andi_tl(op_address, tmp, 0xFF);

    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_zero_page_y(uint8_t imm)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_addi_tl(tmp, cpu_Y, imm);
    tcg_gen_andi_tl(op_address, tmp, 0xFF);

    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_absolute(uint16_t addr)
{
    tcg_gen_movi_tl(op_address, addr);
    nes6502_ld_tl(op_value, op_address, 0, MO_8);
    
    // gen_helper_print_opval(cpu_env, op_value);
    
    // TCGv mem;
    // mem = tcg_temp_new();
    // tcg_gen_movi_tl(mem, 0x2002);
    // tcg_gen_qemu_ld_i32(op_value, mem, 0, MO_32);

    // TCGv mem;
    // mem = tcg_temp_new();
    // tcg_gen_movi_tl(mem, 0x2002);
    // nes6502_ld_tl(op_value, mem, 0, MO_8);

    // tcg_gen_movi_tl(op_address, 0x2000);
    // tcg_gen_qemu_ld_i32(op_value, op_address, 0, MO_32);

    // int val = 0x123456;
    // address_space_read(&address_space_memory, 0x2000,
    //                 MEMTXATTRS_UNSPECIFIED, &val,
    //                 1);

}

static void cpu_address_absolute_x(uint16_t addr)
{
    tcg_gen_addi_tl(op_address, cpu_X, addr);
    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_address_absolute_y(uint16_t addr)
{
    tcg_gen_addi_tl(op_address, cpu_Y, addr);
    tcg_gen_andi_tl(op_address, op_address, 0xFFFF);
    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}

static int cpu_address_relative(DisasContext *ctx, uint8_t imm)
{
    int addr = imm;
    if (addr & 0x80)
        addr -= 0x100;
    tcg_gen_movi_tl(op_address, addr);
    tcg_gen_add_tl(op_address, op_address, cpu_pc);
    return addr;
}

static void cpu_address_indirect(uint16_t addr)
{
    if ((addr & 0xFF) == 0xFF) {
        // Buggy code
        TCGv tmp = tcg_temp_new();
        tcg_gen_movi_tl(tmp, addr & 0xFF00);
        nes6502_ld_tl(tmp, tmp, 0, MO_UB);
        tcg_gen_shli_tl(tmp, tmp, 8);

        TCGv tmp1 = tcg_temp_new();

        tcg_gen_movi_tl(tmp1, addr);
        nes6502_ld_tl(op_address, tmp1, 0, MO_UB);
        tcg_gen_add_tl(op_address, op_address, tmp);
    }
    else {
        // Normal code
        TCGv tmp = tcg_temp_new();
        tcg_gen_movi_tl(tmp, addr);
        nes6502_ld_tl(op_address, tmp, 0, MO_UW);
    }
}

static void cpu_address_indirect_x(uint8_t imm)
{

    TCGv tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, imm);

    tcg_gen_add_tl(tmp, tmp, cpu_X);
    tcg_gen_addi_tl(tmp, tmp, 1);
    tcg_gen_andi_tl(tmp, tmp, 0xFF);

    TCGv val1 = tcg_temp_new();
    nes6502_ld_tl(val1, tmp, 0, MO_UB);
    tcg_gen_shli_tl(val1, val1, 8);

    TCGv val2 = tcg_temp_new();
    tcg_gen_movi_tl(tmp, imm);
    tcg_gen_add_tl(tmp, tmp, cpu_X);
    tcg_gen_andi_tl(tmp, tmp, 0xFF);

    nes6502_ld_tl(val2, tmp, 0, MO_UB);
    tcg_gen_or_tl(op_address, val1, val2);

    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}


static void cpu_address_indirect_y(uint8_t imm)
{

    TCGv tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, imm);
    tcg_gen_addi_tl(tmp, tmp, 1);

    TCGv val1 = tcg_temp_new();
    tcg_gen_andi_tl(tmp, tmp, 0xFF);
    nes6502_ld_tl(val1, tmp, 0, MO_UB);
    tcg_gen_shli_tl(val1, val1, 8);

    TCGv val2 = tcg_temp_new();
    tcg_gen_movi_tl(tmp, imm);
    nes6502_ld_tl(val2, tmp, 0, MO_UB);
    tcg_gen_or_tl(val1, val1, val2);

    tcg_gen_add_tl(val2, val1, cpu_Y);
    tcg_gen_andi_tl(op_address, val2, 0xFFFF);

    nes6502_ld_tl(op_value, op_address, 0, MO_UB);
}

static void cpu_flag_set(TCGv flag, TCGv res, int value)
{
    tcg_gen_setcondi_tl(TCG_COND_EQ, res, flag, value);
}

static void cpu_modify_flag(TCGv flag, TCGv res)
{
    // TCGLabel *t, *done;

    // t = gen_new_label();
    // done = gen_new_label();
    // tcg_gen_brcondi_i32(TCG_COND_EQ, res, 0, t);
    // tcg_gen_movi_i32(flag, 1);
    // tcg_gen_br(done);
    // gen_set_label(t);
    // tcg_gen_movi_i32(flag, 0);
    // gen_set_label(done);

    tcg_gen_andi_tl(res, res, 0xFF);

    tcg_gen_setcondi_tl(TCG_COND_GTU, flag, res, 0);
}

static void cpu_update_z_flags(TCGv r)
{
    // gen_helper_print_zero_flag(cpu_env);
    // gen_helper_print_opval(cpu_env, r);
    TCGLabel *t, *done;

    t = gen_new_label();
    done = gen_new_label();
    tcg_gen_brcondi_i32(TCG_COND_EQ, r, 0, t);
    tcg_gen_movi_i32(cpu_zero_flag, 0);
    tcg_gen_br(done);
    gen_set_label(t);
    tcg_gen_movi_i32(cpu_zero_flag, 1);
    gen_set_label(done);

    //  gen_helper_print_zero_flag(cpu_env);

}

static void cpu_update_n_flags(TCGv r)
{
// gen_helper_print_opval(cpu_env, r);
//     TCGv tt = tcg_temp_new();
//     tcg_gen_movi_i32(tt, 7);
// gen_helper_print_flag(cpu_env, cpu_negative_flag, tt);
    TCGLabel *t, *done;

    t = gen_new_label();
    done = gen_new_label();
    tcg_gen_brcondi_i32(TCG_COND_GTU, r, 127, t);
    tcg_gen_movi_i32(cpu_negative_flag, 0);
    tcg_gen_br(done);
    gen_set_label(t);
    tcg_gen_movi_i32(cpu_negative_flag, 1);
    gen_set_label(done);

// gen_helper_print_flag(cpu_env, cpu_negative_flag, tt);

}

static void cpu_update_zn_flags(TCGv r)
{
    // gen_helper_print_P(cpu_env);
    cpu_update_z_flags(r);
    cpu_update_n_flags(r);
    // gen_helper_print_P(cpu_env);
}

static void nes6502_not_tl(TCGv arg, TCGv target)
{
    TCGLabel *t, *done;

    t = gen_new_label();
    done = gen_new_label();
    tcg_gen_brcondi_i32(TCG_COND_EQ, arg, 0, t);
    tcg_gen_movi_i32(target, 0);
    tcg_gen_br(done);
    gen_set_label(t);
    tcg_gen_movi_i32(target, 1);
    gen_set_label(done);
}

/*
 * Arithmetic Instructions
 */

static void trans_ADC_common(void)
{
    TCGv res = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, res, 1);

    TCGv r = tcg_temp_new();
    tcg_gen_add_tl(r, cpu_A, op_value);
    tcg_gen_add_tl(r, r, res);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, r, 0x100);
    nes6502_not_tl(t, t);
    nes6502_not_tl(t, t);
    cpu_modify_flag(cpu_carry_flag, t);
    
    TCGv t1 = tcg_temp_new();
    TCGv t2 = tcg_temp_new();

    tcg_gen_xor_tl(t1, cpu_A, op_value);
    tcg_gen_xor_tl(t2, cpu_A, r);

    TCGv t3 = tcg_temp_new();
    tcg_gen_andc_tl(t3, t2, t1);
    tcg_gen_andi_tl(t3, t3, 0x80);
    nes6502_not_tl(t3, t3);
    nes6502_not_tl(t3, t3);
    
    cpu_modify_flag(cpu_overflow_flag, t3);

    tcg_gen_andi_tl(cpu_A, r, 0xFF);
    cpu_update_zn_flags(cpu_A);
}

static bool trans_ADC_IM(DisasContext *ctx, arg_ADC_IM *a)
{
    tcg_gen_movi_tl(op_value, a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ZEROPAGE(DisasContext *ctx, arg_ADC_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ZEROPAGE_X(DisasContext *ctx, arg_ADC_ZEROPAGE_X *a)
{
    cpu_address_zero_page(a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ABSOLUTE(DisasContext *ctx, arg_ADC_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ABSOLUTE_X(DisasContext *ctx, arg_ADC_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_ABSOLUTE_Y(DisasContext *ctx, arg_ADC_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_INDIRECT_X(DisasContext *ctx, arg_ADC_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    trans_ADC_common();
    return true;
}

static bool trans_ADC_INDIRECT_Y(DisasContext *ctx, arg_ADC_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    trans_ADC_common();
    return true;
}



static void trans_SBC_common(void)
{
    // gen_helper_print_carry_flag(cpu_env);
    // gen_helper_print_a(cpu_env);
    // gen_helper_print_opval(cpu_env, op_value);

    TCGv res = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, res, 0);

    TCGv r = tcg_temp_new();
    tcg_gen_sub_tl(r, cpu_A, op_value);
    tcg_gen_sub_tl(r, r, res);

    // gen_helper_print_opval(cpu_env, r);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, r, 0x100);

    gen_helper_print_opval(cpu_env, t);

    // tcg_gen_not_tl(t, t);

    TCGv target = tcg_temp_new();
    nes6502_not_tl(t, target);

    gen_helper_print_opval(cpu_env, target);

    cpu_modify_flag(cpu_carry_flag, target);
    
    TCGv t1 = tcg_temp_new();
    TCGv t2 = tcg_temp_new();

    tcg_gen_xor_tl(t1, cpu_A, op_value);
    tcg_gen_xor_tl(t2, cpu_A, r);

    TCGv t3 = tcg_temp_new();
    tcg_gen_and_tl(t3, t2, t1);
    tcg_gen_andi_tl(t3, t3, 0x80);
    nes6502_not_tl(t3, t3);
    nes6502_not_tl(t3, t3);
    
    cpu_modify_flag(cpu_overflow_flag, t3);

    tcg_gen_andi_tl(cpu_A, r, 0xFF);
    cpu_update_zn_flags(cpu_A);

    // gen_helper_print_carry_flag(cpu_env);
}


static bool trans_SBC_IM(DisasContext *ctx, arg_SBC_IM *a)
{
    tcg_gen_movi_tl(op_value, a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ZEROPAGE(DisasContext *ctx, arg_SBC_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ZEROPAGE_X(DisasContext *ctx, arg_SBC_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    trans_SBC_common();

    return true;
}

static bool trans_SBC_ABSOLUTE(DisasContext *ctx, arg_SBC_ABSOLUTE *a)
{
    cpu_address_absolute( a->addr2 <<8 | a->addr1);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ABSOLUTE_X(DisasContext *ctx, arg_SBC_ABSOLUTE_X *a)
{
    cpu_address_absolute_x( a->addr2 <<8 | a->addr1);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_ABSOLUTE_Y(DisasContext *ctx, arg_SBC_ABSOLUTE_Y *a)
{
    cpu_address_absolute_x( a->addr2 <<8 | a->addr1);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_INDIRECT_X(DisasContext *ctx, arg_SBC_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    trans_SBC_common();
    return true;
}

static bool trans_SBC_INDIRECT_Y(DisasContext *ctx, arg_SBC_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    trans_SBC_common();
    return true;
}

static void gen_goto_tb(DisasContext *ctx, int n, target_ulong dest)
{
    const TranslationBlock *tb = ctx->base.tb;

    if (translator_use_goto_tb(&ctx->base, dest)) {
        tcg_gen_goto_tb(n);
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_exit_tb(tb, n);
    } else {
        tcg_gen_movi_i32(cpu_pc, dest);
        tcg_gen_lookup_and_goto_ptr();
    }
    ctx->base.is_jmp = DISAS_NORETURN;
}

/*
 * Branch Instructions
 */


static void cpu_branch(DisasContext *ctx, TCGCond cond, TCGv value, int arg, int addr)
{
    TCGLabel *t, *done;

    t = gen_new_label();
    done = gen_new_label();
    tcg_gen_brcondi_i32(cond, value, arg, t);
    gen_goto_tb(ctx, 0, ctx->base.pc_next);
    tcg_gen_br(done);
    gen_set_label(t);
    gen_goto_tb(ctx, 1, ctx->base.pc_next + addr);
    gen_set_label(done);
}

static bool trans_BCC(DisasContext *ctx, arg_BCC *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, tmp, 0);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr);
    return true;
}

static bool trans_BCS(DisasContext *ctx, arg_BCS *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    // TCGv t = tcg_temp_new();
    // tcg_gen_movi_i32(t, 0);
    // gen_helper_print_flag(cpu_env, cpu_carry_flag, t);
    
    // gen_helper_print_carry_flag(cpu_env);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, tmp, 1);


    // printf("bcs imm %d\n", a->imm);
    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr);
    return true;
}

static bool trans_BEQ(DisasContext *ctx, arg_BEQ *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_zero_flag, tmp, 1);

    gen_helper_print_zero_flag(cpu_env);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr); 
    return true;
}

static bool trans_BMI(DisasContext *ctx, arg_BMI *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_negative_flag, tmp, 1);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr);
    return true;
}

static bool trans_BVS(DisasContext *ctx, arg_BVS *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_overflow_flag, tmp, 1);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr);
    return true;
}

static bool trans_BNE(DisasContext *ctx, arg_BNE *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_zero_flag, tmp, 0);

    TCGv t = tcg_temp_new();
    tcg_gen_movi_i32(t, 1);
    // gen_helper_print_flag(cpu_env, cpu_zero_flag, t);    
    // printf("bne addr %d, pc 0x%x, jmp = 0x%x\n", addr, ctx->base.pc_next, ctx->base.pc_next + addr);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr);
    return true;
}

static bool trans_BPL(DisasContext *ctx, arg_BPL *a)
{
    int res = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    // printf("bpl imm %d\n", a->imm);
    cpu_flag_set(cpu_negative_flag, tmp, 0);

    TCGv t = tcg_temp_new();
    tcg_gen_movi_i32(t, 7);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, res);
    return true;
}

static bool trans_BVC(DisasContext *ctx, arg_BVC *a)
{
    int addr = cpu_address_relative(ctx, a->imm);

    TCGv tmp = tcg_temp_new();
    cpu_flag_set(cpu_overflow_flag, tmp, 0);

    cpu_branch(ctx, TCG_COND_EQ, tmp, 1, addr);
    return true;
}

static bool trans_JMP_ABSOLUTE(DisasContext *ctx, arg_JMP_ABSOLUTE *a)
{
    cpu_address_absolute( a->addr2 <<8 | a->addr1);

    tcg_gen_mov_i32(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_JUMP;
    return true;
}

static bool trans_JMP_INDIRECT(DisasContext *ctx, arg_JMP_INDIRECT *a)
{
    cpu_address_indirect( a->addr2 <<8 | a->addr1);

    tcg_gen_mov_i32(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_JUMP;
    return true;
}

static void cpu_stack_pushw(uint16_t data);
static bool trans_JSR_ABSOLUTE(DisasContext *ctx, arg_JSR_ABSOLUTE *a)
{
    cpu_stack_pushw(ctx->base.pc_next - 1);

    cpu_address_absolute( a->addr2 <<8 | a->addr1);

    tcg_gen_mov_i32(cpu_pc, op_address);
    ctx->base.is_jmp = DISAS_JUMP;
    return true;
}

static void cpu_pack_p(void)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_andi_tl(tmp, P, 0x01);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_carry_flag, tmp, 1);

    tcg_gen_andi_tl(tmp, P, 0x02);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, tmp, 2);

    tcg_gen_andi_tl(tmp, P, 0x04);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_interrupt_flag, tmp, 0x04);

    tcg_gen_andi_tl(tmp, P, 0x08);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_decimal_flag, tmp, 0x08);

    tcg_gen_andi_tl(tmp, P, 0x10);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_break_flag, tmp, 0x10);

    tcg_gen_andi_tl(tmp, P, 0x20);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_unused_flag, tmp, 0x20);

    tcg_gen_andi_tl(tmp, P, 0x40);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_overflow_flag, tmp, 0x40);

    tcg_gen_andi_tl(tmp, P, 0x80);
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_negative_flag, tmp, 0x80);
}

static void nes6502_st_tl(TCGv_i32 val, TCGv addr, TCGArg idx, MemOp memop)
{
    tcg_gen_qemu_st_tl(val, addr, idx, memop);
    gen_helper_print_cpu_ram_st_b(cpu_env, addr);
}

static void cpu_stack_pushb_tcg(TCGv addr, TCGv tmp)
{
    nes6502_st_tl(tmp, addr, 0, MO_UB);
    tcg_gen_subi_tl(cpu_stack_point, cpu_stack_point, 1);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
}

// static void cpu_stack_pushb(uint8_t data)
// {
//     TCGv tmp;
//     tmp = tcg_temp_new();
//     tcg_gen_movi_tl(tmp, data);

//     TCGv addr = tcg_temp_new();
//     tcg_gen_addi_tl(addr, cpu_stack_point, 0x100);
//     cpu_stack_pushb_tcg(addr, tmp);
// }

// static void cpu_stack_pushb(uint8_t data)
// {
//     TCGv tmp;
//     tmp = tcg_temp_new();
//     tcg_gen_movi_tl(tmp, data);

//     TCGv addr = tcg_temp_new();
//     tcg_gen_addi_tl(addr, cpu_stack_point, 0x100);
//     cpu_stack_pushb_tcg(addr, tmp);
// }

static void cpu_stack_pushb_a(TCGv tmp)
{
    // tcg_gen_subi_tl(cpu_stack_point, cpu_stack_point, 1);
    // tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);

    TCGv addr = tcg_temp_new();
    tcg_gen_addi_tl(addr, cpu_stack_point, 0x100);
    tcg_gen_andi_tl(addr, addr, 0xFFFF);
    
    // gen_helper_print_pushb_data(cpu_env, tmp);
    // gen_helper_print_sp(cpu_env);

    cpu_stack_pushb_tcg(addr, tmp);
}

static void cpu_stack_pushw(uint16_t data)
{
    TCGv addr = tcg_temp_new();
    tcg_gen_addi_tl(addr, cpu_stack_point, 0xFF);

    TCGv tmp = tcg_temp_new();
    tcg_gen_movi_tl(tmp, data);

    // tcg_gen_qemu_st_tl(tmp, addr, 0, MO_UB);

    // tcg_gen_addi_tl(addr, addr, 1);
    // tcg_gen_movi_tl(tmp, data>>8);

    // tcg_gen_qemu_st_tl(tmp, addr, 0, MO_UB);
    nes6502_st_tl(tmp, addr, 0, MO_UW);

    // gen_helper_print_pushw_data(cpu_env, tmp);
    // gen_helper_print_sp(cpu_env);

    tcg_gen_subi_tl(cpu_stack_point, cpu_stack_point, 2);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);
}

static void cpu_stack_popb(TCGv val)
{
    TCGv tmp = tcg_temp_new();
    // gen_helper_print_opval(cpu_env, cpu_stack_point);
    tcg_gen_addi_tl(cpu_stack_point, cpu_stack_point, 1);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xff);
    // gen_helper_print_opval(cpu_env, cpu_stack_point);

    tcg_gen_addi_tl(tmp, cpu_stack_point, 0x100);
    // gen_helper_print_opval(cpu_env, tmp);

    tcg_gen_andi_tl(tmp, tmp, 0xFFFF);

    nes6502_ld_tl(val, tmp, 0, MO_UB);

    // gen_helper_print_popb_data(cpu_env, val);
    // gen_helper_print_sp(cpu_env);
}

static void cpu_stack_popw(TCGv val)
{
    TCGv tmp = tcg_temp_new();

    tcg_gen_addi_tl(cpu_stack_point, cpu_stack_point, 2);
    tcg_gen_andi_tl(cpu_stack_point, cpu_stack_point, 0xFF);

    tcg_gen_addi_tl(tmp, cpu_stack_point, 0xff);
    tcg_gen_andi_tl(tmp, tmp, 0xFFFF);

    nes6502_ld_tl(val, tmp, 0, MO_UW);

    // gen_helper_print_popw_data(cpu_env, val);
    // gen_helper_print_sp(cpu_env);
}

static bool trans_RTS(DisasContext *ctx, arg_RTS *a)
{
    TCGv val = tcg_temp_new();
    cpu_stack_popw(val);

    tcg_gen_addi_tl(cpu_pc, val, 1);
    // gen_helper_print_carry_flag(cpu_env);

    ctx->base.is_jmp = DISAS_JUMP;
    return true;
}

static bool trans_RTI(DisasContext *ctx, arg_RTI *a)
{
    cpu_stack_popb(P);
    tcg_gen_ori_tl(P, P, 0x20);
    gen_helper_psw_write(cpu_env, P);

    cpu_stack_popw(cpu_pc);
    ctx->base.is_jmp = DISAS_JUMP;

    return true;
}



/*
 * Data Transfer Instructions
 */

static bool trans_LDA_IM(DisasContext *ctx, arg_LDA_IM *a)
{
    tcg_gen_movi_i32(cpu_A, a->imm);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_ZEROPAGE(DisasContext *ctx, arg_LDA_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    tcg_gen_mov_tl(cpu_A, op_value);
    // gen_helper_print_a(cpu_env);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_ZEROPAGE_X(DisasContext *ctx, arg_LDA_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_ABSOLUTE(DisasContext *ctx, arg_LDA_ABSOLUTE *a)
{
    cpu_address_absolute( a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_A, op_value);
    // gen_helper_print_opval(cpu_env, op_value);
    cpu_update_zn_flags(cpu_A);    
    return true;
}

static bool trans_LDA_ABSOLUTE_X(DisasContext *ctx, arg_LDA_ABSOLUTE_X *a)
{
    cpu_address_absolute_x( a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_A, op_value);

        TCGv tt = tcg_temp_new();
    tcg_gen_movi_i32(tt, 0);
    // gen_helper_print_flag(cpu_env, cpu_carry_flag, tt);

    cpu_update_zn_flags(cpu_A);

    // gen_helper_print_flag(cpu_env, cpu_carry_flag, tt);

    return true;
}

static bool trans_LDA_ABSOLUTE_Y(DisasContext *ctx, arg_LDA_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y( a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_INDIRECT_X(DisasContext *ctx, arg_LDA_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDA_INDIRECT_Y(DisasContext *ctx, arg_LDA_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    tcg_gen_mov_tl(cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_LDX_IM(DisasContext *ctx, arg_LDX_IM *a)
{
    // printf("trans_LDX_IM a->imm%d\n", a->imm);

    tcg_gen_movi_i32(cpu_X, a->imm);
    cpu_update_zn_flags(cpu_X);
    
    return true;
}

static bool trans_LDX_ZEROPAGE(DisasContext *ctx, arg_LDX_ZEROPAGE *a)
{
    printf("trans_LDX_ZEROPAGE\n");
    cpu_address_zero_page(a->imm);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDX_ZEROPAGE_Y(DisasContext *ctx, arg_LDX_ZEROPAGE_Y *a)
{
        printf("trans_LDX_ZEROPAGE_Y\n");
    cpu_address_zero_page_y(a->imm);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDX_ABSOLUTE(DisasContext *ctx, arg_LDX_ABSOLUTE *a)
{
    // printf("trans_LDX_ABSOLUTE\n");
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_X, op_value);
    // gen_helper_print_x(cpu_env);

    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDX_ABSOLUTE_Y(DisasContext *ctx, arg_LDX_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_X, op_value);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_LDY_IM(DisasContext *ctx, arg_LDY_IM *a)
{
    tcg_gen_movi_i32(cpu_Y, a->imm);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ZEROPAGE(DisasContext *ctx, arg_LDY_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ZEROPAGE_X(DisasContext *ctx, arg_LDY_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ABSOLUTE(DisasContext *ctx, arg_LDY_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_LDY_ABSOLUTE_X(DisasContext *ctx, arg_LDY_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    tcg_gen_mov_tl(cpu_Y, op_value);
    cpu_update_zn_flags(cpu_Y);
 
    return true;
}

static bool trans_STA_ZEROPAGE(DisasContext *ctx, arg_STA_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    // gen_helper_print_cpu_ram_b(cpu_env, op_address);
    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);
    // gen_helper_print_a(cpu_env);
    // gen_helper_print_cpu_ram_b(cpu_env, op_address);

    return true;
}

static bool trans_STA_ZEROPAGE_X(DisasContext *ctx, arg_STA_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);
 
    return true;
}

static bool trans_STA_ABSOLUTE(DisasContext *ctx, arg_STA_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 << 8 | a->addr1);

    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_ABSOLUTE_X(DisasContext *ctx, arg_STA_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);
    return true;
}

static bool trans_STA_ABSOLUTE_Y(DisasContext *ctx, arg_STA_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);

    return true;
}

static bool trans_STA_INDIRECT_X(DisasContext *ctx, arg_STA_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);
 
    return true;
}

static bool trans_STA_INDIRECT_Y(DisasContext *ctx, arg_STA_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    nes6502_st_tl(cpu_A, op_address, 0, MO_UB);
 
    return true;
}

static bool trans_STX_ZEROPAGE(DisasContext *ctx, arg_STX_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    nes6502_st_tl(cpu_X, op_address, 0, MO_UB);

    return true;
}

static bool trans_STX_ZEROPAGE_Y(DisasContext *ctx, arg_STX_ZEROPAGE_Y *a)
{
    cpu_address_zero_page_y(a->imm);
    nes6502_st_tl(cpu_X, op_address, 0, MO_UB);
    return true;
}

static bool trans_STX_ABSOLUTE(DisasContext *ctx, arg_STX_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    nes6502_st_tl(cpu_X, op_address, 0, MO_UB);
    return true;
}

static bool trans_STY_ZEROPAGE(DisasContext *ctx, arg_STY_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    nes6502_st_tl(cpu_Y, op_address, 0, MO_UB);
    return true;
}

static bool trans_STY_ZEROPAGE_X(DisasContext *ctx, arg_STY_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    nes6502_st_tl(cpu_Y, op_address, 0, MO_UB);
 
    return true;
}

static bool trans_STY_ABSOLUTE(DisasContext *ctx, arg_STY_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    nes6502_st_tl(cpu_Y, op_address, 0, MO_UB);
    return true;
}

static bool trans_TAX(DisasContext *ctx, arg_TAX *a)
{
    tcg_gen_mov_tl(cpu_X, cpu_A);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_TAY(DisasContext *ctx, arg_TAY *a)
{
    tcg_gen_mov_tl(cpu_Y, cpu_A);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

static bool trans_TSX(DisasContext *ctx, arg_TSX *a)
{
    tcg_gen_mov_tl(cpu_X, cpu_stack_point);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_TXS(DisasContext *ctx, arg_TXS *a)
{
    tcg_gen_mov_tl(cpu_stack_point, cpu_X);
    return true;
}

static bool trans_TXA(DisasContext *ctx, arg_TXA *a)
{
    tcg_gen_mov_tl(cpu_A, cpu_X);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_TYA(DisasContext *ctx, arg_TYA *a)
{
    tcg_gen_mov_tl(cpu_A, cpu_Y);
    cpu_update_zn_flags(cpu_A);
    return true;
}

/*
 * Bit and Bit-test Instructions
 */

static bool trans_AND_IM(DisasContext *ctx, arg_AND_IM *a)
{
    tcg_gen_andi_tl(cpu_A, cpu_A, a->imm);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_AND_ZEROPAGE(DisasContext *ctx, arg_AND_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static bool trans_AND_ZEROPAGE_X(DisasContext *ctx, arg_AND_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static bool trans_AND_ABSOLUTE(DisasContext *ctx, arg_AND_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static bool trans_AND_ABSOLUTE_X(DisasContext *ctx, arg_AND_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static bool trans_AND_ABSOLUTE_Y(DisasContext *ctx, arg_AND_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static bool trans_AND_INDIRECT_X(DisasContext *ctx, arg_AND_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static bool trans_AND_INDIRECT_Y(DisasContext *ctx, arg_AND_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    tcg_gen_and_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);

    return true;
}


static bool trans_EOR_IM(DisasContext *ctx, arg_EOR_IM *a)
{
    tcg_gen_xori_tl(cpu_A, cpu_A, a->imm);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_EOR_ZEROPAGE(DisasContext *ctx, arg_EOR_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    
    return true;
}

static bool trans_EOR_ZEROPAGE_X(DisasContext *ctx, arg_EOR_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_EOR_ABSOLUTE(DisasContext *ctx, arg_EOR_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_EOR_ABSOLUTE_X(DisasContext *ctx, arg_EOR_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_EOR_ABSOLUTE_Y(DisasContext *ctx, arg_EOR_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_EOR_INDIRECT_X(DisasContext *ctx, arg_EOR_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_EOR_INDIRECT_Y(DisasContext *ctx, arg_EOR_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    tcg_gen_xor_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}


static bool trans_ORA_IM(DisasContext *ctx, arg_ORA_IM *a)
{
    tcg_gen_ori_tl(cpu_A, cpu_A, a->imm);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_ZEROPAGE(DisasContext *ctx, arg_ORA_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_ZEROPAGE_X(DisasContext *ctx, arg_ORA_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_ABSOLUTE(DisasContext *ctx, arg_ORA_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_ABSOLUTE_X(DisasContext *ctx, arg_ORA_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_ABSOLUTE_Y(DisasContext *ctx, arg_ORA_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_INDIRECT_X(DisasContext *ctx, arg_ORA_INDIRECT_X *a)
{
    // gen_helper_print_carry_flag(cpu_env);

    cpu_address_indirect_x(a->imm);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ORA_INDIRECT_Y(DisasContext *ctx, arg_ORA_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    tcg_gen_or_tl(cpu_A, cpu_A, op_value);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_ASL_A(DisasContext *ctx, arg_ASL_A *a)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_andi_tl(tmp, cpu_A, 0x80);
    cpu_modify_flag(cpu_carry_flag, tmp);

    tcg_gen_shli_tl(cpu_A, cpu_A, 1);
    tcg_gen_andi_tl(cpu_A, cpu_A, 0xFF);
    cpu_update_zn_flags(cpu_A);

    return true;
}

static void asl_common(void)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_andi_tl(tmp, op_value, 0x80);
    cpu_modify_flag(cpu_carry_flag, tmp); 

    tcg_gen_shli_tl(op_value, op_value, 1);
    tcg_gen_andi_tl(op_value, op_value, 0xFF);
    cpu_update_zn_flags(op_value);
    nes6502_st_tl(op_value, op_address, 0, MO_UB);
}

static bool trans_ASL_ZEROPAGE(DisasContext *ctx, arg_ASL_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    asl_common();
    return true;
}

static bool trans_ASL_ZEROPAGE_X(DisasContext *ctx, arg_ASL_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    asl_common();
    return true;
}

static bool trans_ASL_ABSOLUTE(DisasContext *ctx, arg_ASL_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    asl_common();
    return true;
}

static bool trans_ASL_ABSOLUTE_X(DisasContext *ctx, arg_ASL_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    asl_common();
    return true;
}


static bool trans_LSR_A(DisasContext *ctx, arg_LSR_A *a)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_shri_tl(tmp, cpu_A, 1);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, cpu_A, 0x01);
    cpu_modify_flag(cpu_carry_flag, t);
    tcg_gen_andi_tl(cpu_A, tmp, 0xFF);

    cpu_update_zn_flags(cpu_A);
    return true;
}

static void lsr_common(void)
{
    // gen_helper_print_carry_flag(cpu_env);
    TCGv tmp = tcg_temp_new();
    tcg_gen_andi_tl(tmp, op_value, 0x01);
    cpu_modify_flag(cpu_carry_flag, tmp);

    // gen_helper_print_carry_flag(cpu_env);

    tcg_gen_shri_tl(op_value, op_value, 1);
    tcg_gen_andi_tl(op_value, op_value, 0xFF);

    nes6502_st_tl(op_value, op_address, 0, MO_UB);
    cpu_update_zn_flags(op_value);

}

static bool trans_LSR_ZEROPAGE(DisasContext *ctx, arg_LSR_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    lsr_common();
    return true;
}

static bool trans_LSR_ZEROPAGE_X(DisasContext *ctx, arg_LSR_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    lsr_common();
    return true;
}

static bool trans_LSR_ABSOLUTE(DisasContext *ctx, arg_LSR_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    lsr_common();
    return true;
}

static bool trans_LSR_ABSOLUTE_X(DisasContext *ctx, arg_LSR_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    lsr_common();
    return true;
}

static bool trans_ROL_A(DisasContext *ctx, arg_ROL_A *a)
{
    TCGv tmp = tcg_temp_new();
    tcg_gen_shli_tl(tmp, cpu_A, 1);
    
    TCGv t = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, t, 1);
    
    tcg_gen_or_tl(tmp, tmp, t);

    TCGv v = tcg_temp_new();
    tcg_gen_setcondi_tl(TCG_COND_GT, v, tmp, 0xFF); 
    cpu_modify_flag(cpu_carry_flag, v);

    tcg_gen_andi_tl(cpu_A, tmp, 0xFF);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static void rol_common(void)
{
    tcg_gen_shli_tl(op_value, op_value, 1);
    
    TCGv t = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, t, 1);
    
    tcg_gen_or_tl(op_value, op_value, t);

    TCGv v = tcg_temp_new();
    tcg_gen_setcondi_tl(TCG_COND_GT, v, op_value, 0xFF); 
    cpu_modify_flag(cpu_carry_flag, v);

    tcg_gen_andi_tl(op_value, op_value, 0xFF);
    nes6502_st_tl(op_value, op_address, 0, MO_UB);
    cpu_update_zn_flags(op_value);
}

static bool trans_ROL_ZEROPAGE(DisasContext *ctx, arg_ROL_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    rol_common();
    return true;
}

static bool trans_ROL_ZEROPAGE_X(DisasContext *ctx, arg_ROL_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    rol_common();
    return true;
}

static bool trans_ROL_ABSOLUTE(DisasContext *ctx, arg_ROL_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    rol_common();
    return true;
}

static bool trans_ROL_ABSOLUTE_X(DisasContext *ctx, arg_ROL_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    rol_common();
    return true;
}


static bool trans_ROR_A(DisasContext *ctx, arg_ROR_A *a)
{
    TCGv carry = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, carry, 1);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, cpu_A, 0x1);

    cpu_modify_flag(cpu_carry_flag, t);

    TCGv t1 = tcg_temp_new();
    tcg_gen_shri_tl(t1, cpu_A, 1);

    TCGv t2 = tcg_temp_new();
    tcg_gen_shli_tl(t2, carry, 7);

    tcg_gen_or_tl(cpu_A, t1, t2);

    TCGv t3 = tcg_temp_new();
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, cpu_A, 0);

    cpu_modify_flag(cpu_zero_flag, t3);

    nes6502_not_tl(carry, carry);
    nes6502_not_tl(carry, carry);
    cpu_modify_flag(cpu_negative_flag, carry);

    return true;
}

static void ror_common(void)
{
    TCGv carry = tcg_temp_new();
    cpu_flag_set(cpu_carry_flag, carry, 1);

    TCGv t = tcg_temp_new();
    tcg_gen_andi_tl(t, op_value, 0x1);

    cpu_modify_flag(cpu_carry_flag, t);

    TCGv t1 = tcg_temp_new();
    tcg_gen_shri_tl(t1, op_value, 1);

    TCGv t2 = tcg_temp_new();
    tcg_gen_shli_tl(t2, carry, 7);

    tcg_gen_or_tl(op_value, t1, t2);
    tcg_gen_andi_tl(op_value, op_value, 0xFF);

    TCGv t3 = tcg_temp_new();
    tcg_gen_setcondi_tl(TCG_COND_EQ, t3, op_value, 0);

    cpu_modify_flag(cpu_zero_flag, t3);

    nes6502_not_tl(carry, carry);
    nes6502_not_tl(carry, carry);
    cpu_modify_flag(cpu_negative_flag, carry);

    nes6502_st_tl(op_value, op_address, 0, MO_UB);

}

static bool trans_ROR_ZEROPAGE(DisasContext *ctx, arg_ROR_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    ror_common();
    return true;
}

static bool trans_ROR_ZEROPAGE_X(DisasContext *ctx, arg_ROR_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    ror_common();
    return true;
}

static bool trans_ROR_ABSOLUTE(DisasContext *ctx, arg_ROR_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    ror_common();
    return true;
}

static bool trans_ROR_ABSOLUTE_X(DisasContext *ctx, arg_ROR_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    ror_common();
    return true;
}

// static void print_neg(void)
// {
//     TCGv t = tcg_temp_new();
//     tcg_gen_movi_i32(t, 7);
//     gen_helper_print_flag(cpu_env, cpu_negative_flag, t);
// }

static void bit_common(void)
{
    TCGv t = tcg_temp_new();
    tcg_gen_and_tl(t, cpu_A, op_value);

    nes6502_not_tl(t, t);
    cpu_modify_flag(cpu_zero_flag, t);

    // tcg_gen_andi_tl(t, P, 0x3F);
    // TCGv t2 = tcg_temp_new();
    // tcg_gen_andi_tl(t2, op_value, 0xc0);
    // tcg_gen_or_tl(t, t, t2);
    // tcg_gen_mov_tl(P, t);

    // gen_helper_psw_write(cpu_env, P);

    // print_neg();
    tcg_gen_andi_tl(t, op_value, 0x80);
    cpu_modify_flag(cpu_negative_flag, t);
    // print_neg();

    tcg_gen_andi_tl(t, op_value, 0x40);
    cpu_modify_flag(cpu_overflow_flag, t);
}

static bool trans_BIT_ZEROPAGE(DisasContext *ctx, arg_BIT_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);

    bit_common();
    return true;
}


static bool trans_BIT_ABSOLUTE(DisasContext *ctx, arg_BIT_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);

    bit_common();
    return true;
}


static bool trans_CLC(DisasContext *ctx, arg_CLC *a)
{
    tcg_gen_movi_tl(cpu_carry_flag, 0);
    return true;
}

static bool trans_CLD(DisasContext *ctx, arg_CLD *a)
{
    tcg_gen_movi_tl(cpu_decimal_flag, 0);
    return true;
}

static bool trans_CLI(DisasContext *ctx, arg_CLI *a)
{
    tcg_gen_movi_tl(cpu_interrupt_flag, 0);
    return true;
}

static bool trans_CLV(DisasContext *ctx, arg_CLV *a)
{
    tcg_gen_movi_tl(cpu_overflow_flag, 0);
    return true;
}

static bool trans_SEC(DisasContext *ctx, arg_SEC *a)
{
    tcg_gen_movi_tl(cpu_carry_flag, 1);
    return true;
}

static bool trans_SED(DisasContext *ctx, arg_SED *a)
{
    tcg_gen_movi_tl(cpu_decimal_flag, 1);
    return true;
}

static bool trans_SEI(DisasContext *ctx, arg_SEI *a)
{
    // TCGv mem;
    // mem = tcg_temp_new();
    // tcg_gen_movi_tl(mem, 0x2000);

    // TCGv reg;
    // reg = tcg_temp_new();
    // tcg_gen_movi_tl(reg, 0x10);

    // tcg_gen_qemu_st_tl(reg, mem, 0, MO_8);

    tcg_gen_movi_tl(cpu_interrupt_flag, 1);
    return true;
}

/*
 * Comparison
 */

static void cpu_compare(TCGv reg)
{
    TCGv result = tcg_temp_new();
    tcg_gen_sub_tl(result, reg, op_value);
// gen_helper_print_opval(cpu_env, op_value);
    // TCGv con = tcg_temp_new();
    
    tcg_gen_setcondi_tl(TCG_COND_GE, cpu_carry_flag, result, 0);
    // cpu_modify_flag(cpu_carry_flag, con);
    
    tcg_gen_setcondi_tl(TCG_COND_EQ, cpu_zero_flag, result, 0);
    // cpu_modify_flag(cpu_zero_flag, con);

    tcg_gen_shri_tl(result, result, 7);
    tcg_gen_andi_tl(result, result, 0x1);
    cpu_modify_flag(cpu_negative_flag, result);
}

static bool trans_CMP_IM(DisasContext *ctx, arg_CMP_IM *a)
{
    tcg_gen_movi_i32(op_value, a->imm);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_ZEROPAGE(DisasContext *ctx, arg_CMP_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_ZEROPAGE_X(DisasContext *ctx, arg_CMP_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_ABSOLUTE(DisasContext *ctx, arg_CMP_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_ABSOLUTE_X(DisasContext *ctx, arg_CMP_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_ABSOLUTE_Y(DisasContext *ctx, arg_CMP_ABSOLUTE_Y *a)
{
    cpu_address_absolute_y(a->addr2 <<8 | a->addr1);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_INDIRECT_X(DisasContext *ctx, arg_CMP_INDIRECT_X *a)
{
    cpu_address_indirect_x(a->imm);
    cpu_compare(cpu_A);
    return true;
}

static bool trans_CMP_INDIRECT_Y(DisasContext *ctx, arg_CMP_INDIRECT_Y *a)
{
    cpu_address_indirect_y(a->imm);
    cpu_compare(cpu_A);
    return true;
}


static bool trans_CPX_IM(DisasContext *ctx, arg_CPX_IM *a)
{
    // printf("trans_CPX_IM imm %d\n", a->imm);
    tcg_gen_movi_i32(op_value, a->imm);
    cpu_compare(cpu_X);
    // gen_helper_print_opval(cpu_env, cpu_X);
    return true;
}

static bool trans_CPX_ZEROPAGE(DisasContext *ctx, arg_CPX_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    cpu_compare(cpu_X);

    return true;
}

static bool trans_CPX_ABSOLUTE(DisasContext *ctx, arg_CPX_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    cpu_compare(cpu_X);
    return true;
}



static bool trans_CPY_IM(DisasContext *ctx, arg_CPY_IM *a)
{
    tcg_gen_movi_i32(op_value, a->imm);
    cpu_compare(cpu_Y);

    return true;
}

static bool trans_CPY_ZEROPAGE(DisasContext *ctx, arg_CPY_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    cpu_compare(cpu_Y);
    return true;
}

static bool trans_CPY_ABSOLUTE(DisasContext *ctx, arg_CPY_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    cpu_compare(cpu_Y);
    return true;
}


/*
 * Increment
 */

static void inc_common(void)
{
    TCGv result = tcg_temp_new();
    tcg_gen_addi_tl(result, op_value, 1);
    tcg_gen_andi_tl(result, result, 0xFF);

    nes6502_st_tl(result, op_address, 0, MO_UB);

    cpu_update_zn_flags(result);
}

static bool trans_INC_ZEROPAGE(DisasContext *ctx, arg_INC_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    inc_common();
    return true;
}

static bool trans_INC_ZEROPAGE_X(DisasContext *ctx, arg_INC_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    inc_common();

    return true;
}

static bool trans_INC_ABSOLUTE(DisasContext *ctx, arg_INC_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    inc_common();
    return true;
}

static bool trans_INC_ABSOLUTE_X(DisasContext *ctx, arg_INC_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    inc_common();
    return true;
}


static bool trans_INX(DisasContext *ctx, arg_INX *a)
{
    tcg_gen_addi_tl(cpu_X, cpu_X, 1);
    tcg_gen_andi_tl(cpu_X, cpu_X, 0xFF);
    cpu_update_zn_flags(cpu_X);
    return true;
}

static bool trans_INY(DisasContext *ctx, arg_INY *a)
{
    tcg_gen_addi_tl(cpu_Y, cpu_Y, 1);
    tcg_gen_andi_tl(cpu_Y, cpu_Y, 0xFF);
    cpu_update_zn_flags(cpu_Y);
    return true;
}



/*
 * Decrement
 */

static void dec_common(void)
{
    TCGv result = tcg_temp_new();
    tcg_gen_subi_tl(result, op_value, 1);
    tcg_gen_andi_tl(result, result, 0xFF);

    nes6502_st_tl(result, op_address, 0, MO_UB);
    cpu_update_zn_flags(result);
}

static bool trans_DEC_ZEROPAGE(DisasContext *ctx, arg_DEC_ZEROPAGE *a)
{
    cpu_address_zero_page(a->imm);
    dec_common();
    return true;
}

static bool trans_DEC_ZEROPAGE_X(DisasContext *ctx, arg_DEC_ZEROPAGE_X *a)
{
    cpu_address_zero_page_x(a->imm);
    dec_common();

    return true;
}

static bool trans_DEC_ABSOLUTE(DisasContext *ctx, arg_DEC_ABSOLUTE *a)
{
    cpu_address_absolute(a->addr2 <<8 | a->addr1);
    dec_common();

    return true;
}

static bool trans_DEC_ABSOLUTE_X(DisasContext *ctx, arg_DEC_ABSOLUTE_X *a)
{
    cpu_address_absolute_x(a->addr2 <<8 | a->addr1);
    dec_common();

    return true;
}

static bool trans_DEX(DisasContext *ctx, arg_DEX *a)
{
// gen_helper_print_opval(cpu_env, cpu_X);
    TCGv t = tcg_temp_new();
    tcg_gen_movi_i32(t, 7);
    // gen_helper_print_flag(cpu_env, cpu_negative_flag, t);
    
    tcg_gen_subi_tl(cpu_X, cpu_X, 1);
    tcg_gen_andi_tl(cpu_X, cpu_X, 0xFF);
    cpu_update_zn_flags(cpu_X);

    // gen_helper_print_flag(cpu_env, cpu_negative_flag, t);

    return true;
}

static bool trans_DEY(DisasContext *ctx, arg_DEY *a)
{
    tcg_gen_subi_tl(cpu_Y, cpu_Y, 1);
    tcg_gen_andi_tl(cpu_Y, cpu_Y, 0xFF);
    cpu_update_zn_flags(cpu_Y);
    return true;
}

/*
 * Stack
 */

static bool trans_PHP(DisasContext *ctx, arg_PHP *a)
{
    TCGv status = tcg_temp_new_i32();
    tcg_gen_ori_tl(status, P, 0x30);
    cpu_stack_pushb_a(status);
    return true;
}

static bool trans_PHA(DisasContext *ctx, arg_PHA *a)
{
    cpu_stack_pushb_a(cpu_A);
    // gen_helper_print_opval(cpu_env, cpu_A);
    return true;
}

static bool trans_PLA(DisasContext *ctx, arg_PLA *a)
{
    cpu_stack_popb(cpu_A);
    cpu_update_zn_flags(cpu_A);
    return true;
}

static bool trans_PLP(DisasContext *ctx, arg_PLP *a)
{
    TCGv t = tcg_temp_new();
    cpu_stack_popb(t);

    tcg_gen_andi_tl(t, t, 0xEF);
    tcg_gen_ori_tl(t, t, 0x20);

    tcg_gen_mov_tl(P, t);

    cpu_pack_p();
    return true;
}

/*
 * MCU Control Instructions
 */

static bool trans_NOP(DisasContext *ctx, arg_NOP *a)
{
    return true;
}

static bool trans_BRK(DisasContext *ctx, arg_BRK *a)
{
    cpu_stack_pushw(ctx->base.pc_next - 1);
    cpu_stack_pushb_a(P);
    tcg_gen_ori_tl(P, P, 0x20);
    tcg_gen_ori_tl(P, P, 0x10);

    TCGv nmi_addr = tcg_temp_new();
    tcg_gen_movi_tl(nmi_addr, 0xFFFA);
    nes6502_ld_tl(cpu_pc, nmi_addr, 0, MO_UW);
    ctx->base.is_jmp = DISAS_JUMP;

    // cpu_stack_popw(cpu_pc);
    // cpu_stack_popb(cpu_pc);
    // cpu_stack_pushb(0);
    return true;
}


/*
 *  Core translation mechanism functions:
 *
 *    - translate()
 *    - canonicalize_skip()
 *    - gen_intermediate_code()
 *    - restore_state_to_opc()
 *
 */
static void translate(DisasContext *ctx)
{
    uint32_t opcode;
    target_long npc_t = ctx->base.pc_next;
    opcode = decode_insn_load(ctx);
    uint16_t op = opcode >> 24;

/*
opcode 0xa5 pc 0xc1e1 LDA zero_page
opcode 0xf0 pc 0xc1e3 BEQ relative
*/
    printf("opcode 0x%x pc 0x%x %s %s\n", op, npc_t, cpu_op_name[op], cpu_op_address_mode[op]);
    fprintf(g_fp, "opcode 0x%x pc 0x%x %s %s\n", op, npc_t, cpu_op_name[op], cpu_op_address_mode[op]);
    if (!decode_insn(ctx, opcode)) {
        gen_helper_unsupported(cpu_env);
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}

static void nes6502_tr_init_disas_context(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPUNES6502State *env = cs->env_ptr;

    ctx->cs = cs;
    ctx->env = env;
    ctx->npc = ctx->base.pc_first;
}

static void nes6502_tr_tb_start(DisasContextBase *db, CPUState *cs)
{
}

static void nes6502_tr_insn_start(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(ctx->npc);
}

static void nes6502_tr_translate_insn(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    ctx->npc = ctx->base.pc_next;
    translate(ctx);
}

static void nes6502_tr_tb_stop(DisasContextBase *dcbase, CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
    case DISAS_NORETURN:
        break;
    case DISAS_NEXT:
    case DISAS_TOO_MANY:
        gen_goto_tb(ctx, 0, dcbase->pc_next);
        break;
    case DISAS_JUMP:
        tcg_gen_lookup_and_goto_ptr();
        break;
    case DISAS_EXIT:
        tcg_gen_exit_tb(NULL, 0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void nes6502_tr_disas_log(const DisasContextBase *dcbase,
                             CPUState *cs, FILE *logfile)
{
    fprintf(logfile, "IN: %s\n", lookup_symbol(dcbase->pc_first));
    target_disas(logfile, cs, dcbase->pc_first, dcbase->tb->size);
}

static const TranslatorOps nes6502_tr_ops = {
    .init_disas_context = nes6502_tr_init_disas_context,
    .tb_start           = nes6502_tr_tb_start,
    .insn_start         = nes6502_tr_insn_start,
    .translate_insn     = nes6502_tr_translate_insn,
    .tb_stop            = nes6502_tr_tb_stop,
    .disas_log          = nes6502_tr_disas_log,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb, int *max_insns,
                           target_ulong pc, void *host_pc)
{
    DisasContext dc = { };
    init_log();
    translator_loop(cs, tb, max_insns, pc, host_pc, &nes6502_tr_ops, &dc.base);
}
