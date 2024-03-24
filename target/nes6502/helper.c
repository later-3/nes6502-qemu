/*
 * QEMU AVR CPU helpers
 *
 * Copyright (c) 2016-2020 Michael Rolnik
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
#include "qemu/log.h"
#include "cpu.h"
#include "hw/core/tcg-cpu-ops.h"
#include "exec/exec-all.h"
#include "exec/address-spaces.h"
#include "exec/helper-proto.h"
#include "litenes_cpu.h"

static const char *flag_arr[8] = {
    "carry_flag",
    "zero_flag",
    "interrupt_flag",
    "decimal_flag",
    "break_flag",
    "unused_flag",
    "overflow_flag",
    "negative_flag"
}; 

void helper_print_opval(CPUNES6502State *env, uint32_t val)
{
    printf("print_val 0x%x\n", val);
}

void helper_print_flag(CPUNES6502State *env, uint32_t val, uint32_t index)
{
    printf("print_flag val 0x%x, flag %s\n", val, flag_arr[index]);
}

void helper_print_sp(CPUNES6502State *env)
{
    printf("print_sp 0x%x\n", env->stack_point);
}

void helper_print_x(CPUNES6502State *env)
{
    printf("print_x 0x%x\n", env->reg_X);
}

void helper_print_zero_flag(CPUNES6502State *env)
{
    // printf("print_zero_flag 0x%x\n", env->zero_flag);
    // fprintf(g_fp, "zero_flag 0x%x\n", env->zero_flag);
}

void helper_print_carry_flag(CPUNES6502State *env)
{
    // printf("print_zero_flag 0x%x\n", env->zero_flag);
    fprintf(g_fp, "carry_flag 0x%x\n", env->carry_flag);
}

void helper_print_a(CPUNES6502State *env)
{
    printf("print_a 0x%x\n", env->reg_A);
    fprintf(g_fp, "registe A 0x%x\n", env->reg_A);
}

void helper_print_cpu_ram_b(CPUNES6502State *env, uint32_t ind)
{
    if (ind != 0) {
        return;
    }
    int val = cpu_ldub_data(env, ind);
    // printf("print_a 0x%x\n", env->reg_A);
    fprintf(g_fp, "cpu_ram_b ind: 0x%x, val_b: 0x%x\n", ind, val);
    printf("cpu_ram_b ind: 0x%x, val_b: 0x%x\n", ind, val);
}

void helper_print_pushb_data(CPUNES6502State *env, uint32_t val)
{
    printf("print_pushb data 0x%x\n", val);
}

void helper_print_pushw_data(CPUNES6502State *env, uint32_t val)
{
    printf("print_pushw data 0x%x\n", val);
}

void helper_print_popb_data(CPUNES6502State *env, uint32_t val)
{
    printf("print_popb data 0x%x\n", val);
}

void helper_print_popw_data(CPUNES6502State *env, uint32_t val)
{
    printf("print_popw data 0x%x\n", val);
}

void helper_print_P(CPUNES6502State *env)
{
    printf("flag name %s, val 0x%x, \n", flag_arr[0], env->carry_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[1], env->zero_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[2], env->interrupt_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[3], env->decimal_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[4], env->break_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[5], env->unused_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[6], env->overflow_flag);
    printf("flag name %s, val 0x%x, \n", flag_arr[7], env->negative_flag);
    printf("\n");
}

uint32_t helper_psw_read(CPUNES6502State *env)
{
    uint32_t p = 0;
    p |= env->carry_flag;
    p |= env->zero_flag << 1;
    p |= env->interrupt_flag << 2;
    p |= env->decimal_flag << 3;
    p |= env->break_flag << 4;
    p |= env->unused_flag << 5;
    p |= env->overflow_flag << 6;
    p |= env->negative_flag << 7;
    env->P = p;
    return p;
}

void helper_psw_write(CPUNES6502State *env, uint32_t val)
{
    // carry_bp      = 0,
    // zero_bp       = 1,
    // interrupt_bp  = 2,
    // decimal_bp    = 3,
    // break_bp      = 4,
    // unused_bp     = 5,
    // overflow_bp   = 6,
    // negative_bp   = 7
    env->carry_flag = val & 0x1;
    env->zero_flag = (val & 0x2) ? 1 : 0;
    env->interrupt_flag = (val & 0x4) ? 1 : 0;
    env->decimal_flag = (val & 0x8) ? 1 : 0;
    env->break_flag = (val & 0x10) ? 1 : 0;
    env->unused_flag = (val & 0x20) ? 1 : 0;
    env->overflow_flag = (val & 0x40) ? 1 : 0;
    env->negative_flag = (val & 0x80) ? 1 : 0;
    env->P = val & 0xFF;
}

bool nes6502_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    if (!(interrupt_request & CPU_INTERRUPT_HARD)) {
        return false;   
    }

    assert(env->carry_flag < 2);
    assert(env->zero_flag < 2);
    assert(env->interrupt_flag < 2);
    assert(env->decimal_flag < 2);
    assert(env->break_flag < 2);
    assert(env->unused_flag < 2);
    assert(env->overflow_flag < 2);
    assert(env->negative_flag < 2);

    env->interrupt_flag = 1;
    env->unused_flag = 0;
    
    env->P = env->carry_flag | env->zero_flag << 1 | env->interrupt_flag << 2 \
                | env->decimal_flag << 3 | env->break_flag << 4 | env->unused_flag << 5 \
                | env->overflow_flag << 6 | env->negative_flag << 7;
    
    unsigned int sp =  env->stack_point + 0xFF;
    sp &= 0xffff;
    cpu_stw_data(env, sp, env->pc_w);
    
    env->stack_point -= 2;
    env->stack_point &= 0xff;

    sp = env->stack_point + 0x100;
    env->stack_point -= 1;
    env->stack_point &= 0xff;

    cpu_stb_data(env, sp, env->P);

    env->pc_w = cpu_lduw_data(env, 0xFFFA);
    cs->interrupt_request &= ~CPU_INTERRUPT_HARD;
    return true;
}

void avr_cpu_do_interrupt(CPUState *cs)
{
    // NES6502CPU *cpu = NES6502_CPU(cs);
    // CPUNES6502State *env = &cpu->env;

    // int vector = 0;
    // int base = 0;
    // env->pc_w = base + vector;
    // env->sregI = 0; /* clear Global Interrupt Flag */

    // cs->exception_index = -1;
}

hwaddr avr_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    return addr; /* I assume 1:1 address correspondence */
}

bool nes6502_cpu_tlb_fill(CPUState *cs, vaddr addr, int size,
                      MMUAccessType access_type, int mmu_idx,
                      bool probe, uintptr_t retaddr)
{
    uint32_t address, physical, prot;

    /* Linear mapping */
    address = physical = addr & TARGET_PAGE_MASK;
    prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    tlb_set_page(cs, address, physical, prot, mmu_idx, TARGET_PAGE_SIZE);
    return true;
}

/*
 *  helpers
 */

void helper_sleep(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_HLT;
    cpu_loop_exit(cs);
}

void helper_unsupported(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    /*
     *  I count not find what happens on the real platform, so
     *  it's EXCP_DEBUG for meanwhile
     */
    cs->exception_index = EXCP_DEBUG;
    if (qemu_loglevel_mask(LOG_UNIMP)) {
        qemu_log("UNSUPPORTED\n");
        cpu_dump_state(cs, stderr, 0);
    }
    cpu_loop_exit(cs);
}

void helper_debug(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

void helper_break(CPUNES6502State *env)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = EXCP_DEBUG;
    cpu_loop_exit(cs);
}

void helper_wdr(CPUNES6502State *env)
{
    qemu_log_mask(LOG_UNIMP, "WDG reset (not implemented)\n");
}

/*
 * This function implements IN instruction
 *
 * It does the following
 * a.  if an IO register belongs to CPU, its value is read and returned
 * b.  otherwise io address is translated to mem address and physical memory
 *     is read.
 * c.  it caches the value for sake of SBI, SBIC, SBIS & CBI implementation
 *
 */
target_ulong helper_inb(CPUNES6502State *env, uint32_t port)
{
    target_ulong data = 0;

    switch (port) {
    case 0x38: /* RAMPD */
        data = 0xff & (env->rampD >> 16);
        break;
    case 0x39: /* RAMPX */
        data = 0xff & (env->rampX >> 16);
        break;
    case 0x3a: /* RAMPY */
        data = 0xff & (env->rampY >> 16);
        break;
    case 0x3b: /* RAMPZ */
        data = 0xff & (env->rampZ >> 16);
        break;
    case 0x3c: /* EIND */
        data = 0xff & (env->eind >> 16);
        break;
    case 0x3d: /* SPL */
        data = env->sp & 0x00ff;
        break;
    case 0x3e: /* SPH */
        data = env->sp >> 8;
        break;
    case 0x3f: /* SREG */
        data = cpu_get_sreg(env);
        break;
    default:
        /* not a special register, pass to normal memory access */
        data = address_space_ldub(&address_space_memory,
                                  OFFSET_IO_REGISTERS + port,
                                  MEMTXATTRS_UNSPECIFIED, NULL);
    }

    return data;
}

/*
 *  This function implements OUT instruction
 *
 *  It does the following
 *  a.  if an IO register belongs to CPU, its value is written into the register
 *  b.  otherwise io address is translated to mem address and physical memory
 *      is written.
 *  c.  it caches the value for sake of SBI, SBIC, SBIS & CBI implementation
 *
 */
void helper_outb(CPUNES6502State *env, uint32_t port, uint32_t data)
{
    data &= 0x000000ff;
}

/*
 *  this function implements LD instruction when there is a possibility to read
 *  from a CPU register
 */
target_ulong helper_fullrd(CPUNES6502State *env, uint32_t addr)
{
    uint8_t data;

    env->fullacc = false;

    if (addr < NUMBER_OF_CPU_REGISTERS) {
        /* CPU registers */
        data = env->r[addr];
    } else if (addr < NUMBER_OF_CPU_REGISTERS + NUMBER_OF_IO_REGISTERS) {
        /* IO registers */
        data = helper_inb(env, addr - NUMBER_OF_CPU_REGISTERS);
    } else {
        /* memory */
        data = address_space_ldub(&address_space_memory, OFFSET_DATA + addr,
                                  MEMTXATTRS_UNSPECIFIED, NULL);
    }
    return data;
}

/*
 *  this function implements ST instruction when there is a possibility to write
 *  into a CPU register
 */
void helper_fullwr(CPUNES6502State *env, uint32_t data, uint32_t addr)
{
    env->fullacc = false;

    /* Following logic assumes this: */
    assert(OFFSET_CPU_REGISTERS == OFFSET_DATA);
    assert(OFFSET_IO_REGISTERS == OFFSET_CPU_REGISTERS +
                                  NUMBER_OF_CPU_REGISTERS);

    if (addr < NUMBER_OF_CPU_REGISTERS) {
        /* CPU registers */
        env->r[addr] = data;
    } else if (addr < NUMBER_OF_CPU_REGISTERS + NUMBER_OF_IO_REGISTERS) {
        /* IO registers */
        helper_outb(env, addr - NUMBER_OF_CPU_REGISTERS, data);
    } else {
        /* memory */
        address_space_stb(&address_space_memory, OFFSET_DATA + addr, data,
                          MEMTXATTRS_UNSPECIFIED, NULL);
    }
}
