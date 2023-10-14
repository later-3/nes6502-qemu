/*
 * QEMU NES6502 CPU
 *
 * Copyright (c) 2023- Joey
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

#ifndef QEMU_NES6502_CPU_H
#define QEMU_NES6502_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-defs.h"

#ifdef CONFIG_USER_ONLY
#error "AVR 8-bit does not support user mode"
#endif

#define NES6502_CPU_TYPE_SUFFIX "-" TYPE_NES6502_CPU
#define NES6502_CPU_TYPE_NAME(name) (name NES6502_CPU_TYPE_SUFFIX)
#define CPU_RESOLVING_TYPE TYPE_NES6502_CPU

#define TCG_GUEST_DEFAULT_MO 0

/*
 * AVR has two memory spaces, data & code.
 * e.g. both have 0 address
 * ST/LD instructions access data space
 * LPM/SPM and instruction fetching access code memory space
 */
#define MMU_CODE_IDX 0
#define MMU_DATA_IDX 1

#define EXCP_RESET 1
#define EXCP_INT(n) (EXCP_RESET + (n) + 1)

/* Number of CPU registers */
#define NUMBER_OF_CPU_REGISTERS 32
/* Number of IO registers accessible by ld/st/in/out */
#define NUMBER_OF_IO_REGISTERS 64

/*
 * Offsets of AVR memory regions in host memory space.
 *
 * This is needed because the AVR has separate code and data address
 * spaces that both have start from zero but have to go somewhere in
 * host memory.
 *
 * It's also useful to know where some things are, like the IO registers.
 */
/* program memory */
#define RAM_ADDR 0x0
#define FIRST_CODE_OFFSET 0x8000
#define SECOND_CODE_OFFSET 0xC000
#define PRG_BLOGK_SIZE 0x4000

/* CPU registers, IO registers, and SRAM */
#define OFFSET_DATA 0x00800000
/* CPU registers specifically, these are mapped at the start of data */
#define OFFSET_CPU_REGISTERS OFFSET_DATA
/*
 * IO registers, including status register, stack pointer, and memory
 * mapped peripherals, mapped just after CPU registers
 */
#define OFFSET_IO_REGISTERS (OFFSET_DATA + NUMBER_OF_CPU_REGISTERS)

typedef struct CPUArchState {
    uint32_t pc_w; /* 0x003fffff up to 22 bits */

    uint32_t sregC; /* 0x00000001 1 bit */
    uint32_t sregZ; /* 0x00000001 1 bit */
    uint32_t sregN; /* 0x00000001 1 bit */
    uint32_t sregV; /* 0x00000001 1 bit */
    uint32_t sregS; /* 0x00000001 1 bit */
    uint32_t sregH; /* 0x00000001 1 bit */
    uint32_t sregT; /* 0x00000001 1 bit */
    uint32_t sregI; /* 0x00000001 1 bit */

    uint32_t rampD; /* 0x00ff0000 8 bits */
    uint32_t rampX; /* 0x00ff0000 8 bits */
    uint32_t rampY; /* 0x00ff0000 8 bits */
    uint32_t rampZ; /* 0x00ff0000 8 bits */
    uint32_t eind; /* 0x00ff0000 8 bits */

    uint32_t r[NUMBER_OF_CPU_REGISTERS]; /* 8 bits each */
    uint32_t sp; /* 16 bits */

    uint32_t skip; /* if set skip instruction */

    uint64_t intsrc; /* interrupt sources */
    bool fullacc; /* CPU/MEM if true MEM only otherwise */

    uint64_t features;

    uint32_t reg_A;
    uint32_t reg_X;
    uint32_t reg_Y;
    uint32_t carry_flag; /* 0x00000001 1 bit */
    uint32_t zero_flag; /* 0x00000001 1 bit */
    uint32_t interrupt_flag; /* 0x00000001 1 bit */
    uint32_t decimal_flag; /* 0x00000001 1 bit */
    uint32_t break_flag; /* 0x00000001 1 bit */
    uint32_t unused_flag; /* 0x00000001 1 bit */
    uint32_t overflow_flag; /* 0x00000001 1 bit */
    uint32_t negative_flag; /* 0x00000001 1 bit */
    uint32_t P;
    uint32_t stack_point; /* 16 bits */

    uint32_t op_address;
    uint32_t op_value;
} CPUNES6502State;

/**
 *  NES6502CPU:
 *  @env: #CPUNES6502State
 *
 *  A NES6502 CPU.
 */
struct ArchCPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/

    CPUNegativeOffsetState neg;
    CPUNES6502State env;
};

extern const struct VMStateDescription vms_avr_cpu;

void avr_cpu_do_interrupt(CPUState *cpu);
bool avr_cpu_exec_interrupt(CPUState *cpu, int int_req);
hwaddr avr_cpu_get_phys_page_debug(CPUState *cpu, vaddr addr);
int avr_cpu_gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
int avr_cpu_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);
int nes6502_print_insn(bfd_vma addr, disassemble_info *info);
vaddr avr_cpu_gdb_adjust_breakpoint(CPUState *cpu, vaddr addr);

#define cpu_list nes6502_cpu_list
#define cpu_mmu_index avr_cpu_mmu_index

static inline int avr_cpu_mmu_index(CPUNES6502State *env, bool ifetch)
{
    return ifetch ? MMU_CODE_IDX : MMU_DATA_IDX;
}

void nes6502_cpu_tcg_init(void);

void nes6502_cpu_list(void);

enum {
    TB_FLAGS_FULL_ACCESS = 1,
    TB_FLAGS_SKIP = 2,
};

static inline void cpu_get_tb_cpu_state(CPUNES6502State *env, target_ulong *pc,
                                        target_ulong *cs_base, uint32_t *pflags)
{
    *pc = env->pc_w;
}

static inline int cpu_interrupts_enabled(CPUNES6502State *env)
{
    return env->sregI != 0;
}

static inline uint8_t cpu_get_sreg(CPUNES6502State *env)
{
    return (env->sregC) << 0
         | (env->sregZ) << 1
         | (env->sregN) << 2
         | (env->sregV) << 3
         | (env->sregS) << 4
         | (env->sregH) << 5
         | (env->sregT) << 6
         | (env->sregI) << 7;
}

static inline void cpu_set_sreg(CPUNES6502State *env, uint8_t sreg)
{
    env->sregC = (sreg >> 0) & 0x01;
    env->sregZ = (sreg >> 1) & 0x01;
    env->sregN = (sreg >> 2) & 0x01;
    env->sregV = (sreg >> 3) & 0x01;
    env->sregS = (sreg >> 4) & 0x01;
    env->sregH = (sreg >> 5) & 0x01;
    env->sregT = (sreg >> 6) & 0x01;
    env->sregI = (sreg >> 7) & 0x01;
}

bool nes6502_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                      MMUAccessType access_type, int mmu_idx,
                      bool probe, uintptr_t retaddr);

#include "exec/cpu-all.h"

#endif /* QEMU_NES6502_CPU_H */
