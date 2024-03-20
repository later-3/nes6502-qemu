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
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "exec/exec-all.h"
#include "cpu.h"
#include "disas/dis-asm.h"

static void nes6502_cpu_set_pc(CPUState *cs, vaddr value)
{
    NES6502CPU *cpu = NES6502_CPU(cs);

    cpu->env.pc_w = value; /* internally PC points to words */
}

static vaddr nes6502_cpu_get_pc(CPUState *cs)
{
    NES6502CPU *cpu = NES6502_CPU(cs);

    return cpu->env.pc_w;
}

static bool avr_cpu_has_work(CPUState *cs)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    return (cs->interrupt_request & (CPU_INTERRUPT_HARD | CPU_INTERRUPT_RESET))
            && cpu_interrupts_enabled(env);
}

static void avr_cpu_synchronize_from_tb(CPUState *cs,
                                        const TranslationBlock *tb)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    tcg_debug_assert(!(cs->tcg_cflags & CF_PCREL));
    env->pc_w = tb->pc; /* internally PC points to words */
}

static void avr_restore_state_to_opc(CPUState *cs,
                                     const TranslationBlock *tb,
                                     const uint64_t *data)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    env->pc_w = data[0];
}

static void nes6502_cpu_reset_hold(Object *obj)
{
    CPUState *cs = CPU(obj);
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;

    env->stack_point = 253;
    env->P = 0x24;
    env->P |= 0x4;

    env->pc_w = 0;
    env->sp = 0;

    memset(env->r, 0, sizeof(env->r));

    env->reg_A = 0;
    env->reg_X = 0;
    env->reg_Y = 0;

    env->carry_flag = 0;
    env->zero_flag = 0;
    env->interrupt_flag = 1;
    env->decimal_flag = 0;
    env->break_flag = 0;
    env->unused_flag = 1;
    env->overflow_flag = 0;
    env->negative_flag = 0;
}

static void nes6502_cpu_disas_set_info(CPUState *cpu, disassemble_info *info)
{
    info->mach = bfd_arch_avr;
    info->print_insn = nes6502_print_insn;
}

static void nes6502_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    NES6502CPUClass *mcc = NES6502_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
    qemu_init_vcpu(cs);
    cpu_reset(cs);

    mcc->parent_realize(dev, errp);
}

static void nes6502_cpu_set_int(void *opaque, int irq, int level)
{
    NES6502CPU *cpu = opaque;
    CPUNES6502State *env = &cpu->env;
    CPUState *cs = CPU(cpu);
    uint64_t mask = (1ull << irq);

    if (level) {
        env->intsrc |= mask;
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        env->intsrc &= ~mask;
        if (env->intsrc == 0) {
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
    }
}

static void nes6502_cpu_initfn(Object *obj)
{
    NES6502CPU *cpu = NES6502_CPU(obj);

    cpu_set_cpustate_pointers(cpu);

    /* Set the number of interrupts supported by the CPU. */
    qdev_init_gpio_in(DEVICE(cpu), nes6502_cpu_set_int,
                      sizeof(cpu->env.intsrc) * 8);
}

static ObjectClass *avr_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;

    oc = object_class_by_name(cpu_model);
    if (object_class_dynamic_cast(oc, TYPE_NES6502_CPU) == NULL ||
        object_class_is_abstract(oc)) {
        oc = NULL;
    }
    return oc;
}

static void avr_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    NES6502CPU *cpu = NES6502_CPU(cs);
    CPUNES6502State *env = &cpu->env;
    int i;

    qemu_fprintf(f, "\n");
    qemu_fprintf(f, "PC:    %06x\n", env->pc_w * 2); /* PC points to words */
    qemu_fprintf(f, "SP:      %04x\n", env->sp);
    qemu_fprintf(f, "rampD:     %02x\n", env->rampD >> 16);
    qemu_fprintf(f, "rampX:     %02x\n", env->rampX >> 16);
    qemu_fprintf(f, "rampY:     %02x\n", env->rampY >> 16);
    qemu_fprintf(f, "rampZ:     %02x\n", env->rampZ >> 16);
    qemu_fprintf(f, "EIND:      %02x\n", env->eind >> 16);
    qemu_fprintf(f, "X:       %02x%02x\n", env->r[27], env->r[26]);
    qemu_fprintf(f, "Y:       %02x%02x\n", env->r[29], env->r[28]);
    qemu_fprintf(f, "Z:       %02x%02x\n", env->r[31], env->r[30]);
    qemu_fprintf(f, "SREG:    [ %c %c %c %c %c %c %c %c ]\n",
                 env->sregI ? 'I' : '-',
                 env->sregT ? 'T' : '-',
                 env->sregH ? 'H' : '-',
                 env->sregS ? 'S' : '-',
                 env->sregV ? 'V' : '-',
                 env->sregN ? '-' : 'N', /* Zf has negative logic */
                 env->sregZ ? 'Z' : '-',
                 env->sregC ? 'I' : '-');
    qemu_fprintf(f, "SKIP:    %02x\n", env->skip);

    qemu_fprintf(f, "\n");
    for (i = 0; i < ARRAY_SIZE(env->r); i++) {
        qemu_fprintf(f, "R[%02d]:  %02x   ", i, env->r[i]);

        if ((i % 8) == 7) {
            qemu_fprintf(f, "\n");
        }
    }
    qemu_fprintf(f, "\n");
}

#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps avr_sysemu_ops = {
    .get_phys_page_debug = avr_cpu_get_phys_page_debug,
};

#include "hw/core/tcg-cpu-ops.h"

static const struct TCGCPUOps nes6502_tcg_ops = {
    .initialize = nes6502_cpu_tcg_init,
    .synchronize_from_tb = avr_cpu_synchronize_from_tb,
    .restore_state_to_opc = avr_restore_state_to_opc,
    .cpu_exec_interrupt = nes6502_cpu_exec_interrupt,
    .tlb_fill = nes6502_cpu_tlb_fill,
    .do_interrupt = avr_cpu_do_interrupt,
};

static void nes6502_cpu_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    CPUClass *cc = CPU_CLASS(oc);
    NES6502CPUClass *mcc = NES6502_CPU_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, nes6502_cpu_realizefn, &mcc->parent_realize);

    resettable_class_set_parent_phases(rc, NULL, nes6502_cpu_reset_hold, NULL,
                                       &mcc->parent_phases);

    cc->class_by_name = avr_cpu_class_by_name;

    cc->has_work = avr_cpu_has_work;
    cc->dump_state = avr_cpu_dump_state;
    cc->set_pc = nes6502_cpu_set_pc;
    cc->get_pc = nes6502_cpu_get_pc;
    dc->vmsd = &vms_avr_cpu;
    cc->sysemu_ops = &avr_sysemu_ops;
    cc->disas_set_info = nes6502_cpu_disas_set_info;
    cc->gdb_read_register = avr_cpu_gdb_read_register;
    cc->gdb_write_register = avr_cpu_gdb_write_register;
    cc->gdb_adjust_breakpoint = avr_cpu_gdb_adjust_breakpoint;
    cc->gdb_num_core_regs = 35;
    cc->gdb_core_xml_file = "avr-cpu.xml";
    cc->tcg_ops = &nes6502_tcg_ops;
}

typedef struct NES6502CPUInfo {
    const char *name;
    void (*initfn)(Object *obj);
} NES6502CPUInfo;


static void nes6502_cpu_list_entry(gpointer data, gpointer user_data)
{
    const char *typename = object_class_get_name(OBJECT_CLASS(data));

    qemu_printf("%s\n", typename);
}

void nes6502_cpu_list(void)
{
    GSList *list;
    list = object_class_get_list_sorted(TYPE_NES6502_CPU, false);
    g_slist_foreach(list, nes6502_cpu_list_entry, NULL);
    g_slist_free(list);
}

static const TypeInfo avr_cpu_type_info[] = {
    {
        .name = TYPE_NES6502_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(NES6502CPU),
        .instance_init = nes6502_cpu_initfn,
        .class_size = sizeof(NES6502CPUClass),
        .class_init = nes6502_cpu_class_init,
    },
};

DEFINE_TYPES(avr_cpu_type_info)
