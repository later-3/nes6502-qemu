/*
 * QEMU Arduino boards
 *
 * Copyright (c) 2019-2020 Philippe Mathieu-Daudé
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* TODO: Implement the use of EXTRAM */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "nesmcu.h"
#include "boot.h"
#include "qom/object.h"

struct XiaoBaWangMachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/
    NesMcuState mcu;
};
typedef struct XiaoBaWangMachineState XiaoBaWangMachineState;

struct XiaoBaWangMachineClass {
    /*< private >*/
    MachineClass parent_class;
    /*< public >*/
    const char *mcu_type;
    uint64_t xtal_hz;
};
typedef struct XiaoBaWangMachineClass XiaoBaWangMachineClass;

#define TYPE_XIAOBAWANG_MACHINE \
        MACHINE_TYPE_NAME("xiaobawang")
DECLARE_OBJ_CHECKERS(XiaoBaWangMachineState, XiaoBaWangMachineClass,
                     XIAOBAWANG_MACHINE, TYPE_XIAOBAWANG_MACHINE)

static void arduino_mega2560_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    XiaoBaWangMachineClass *amc = XIAOBAWANG_MACHINE_CLASS(mc);

    mc->desc        = "xiaobawang game machine";
    mc->alias       = "nes game machine";
    amc->mcu_type   = TYPE_NES6502_MCU;
    amc->xtal_hz    = 16 * 1000 * 1000; /* CSTCE16M0V53-R0 */
};

static
void xiaobawang_init(MachineState *machine)
{
    XiaoBaWangMachineClass *amc = XIAOBAWANG_MACHINE_GET_CLASS(machine);
    XiaoBaWangMachineState *ams = XIAOBAWANG_MACHINE(machine);

    object_initialize_child(OBJECT(machine), "mcu", &ams->mcu, amc->mcu_type);
    object_property_set_uint(OBJECT(&ams->mcu), "xtal-frequency-hz",
                             amc->xtal_hz, &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&ams->mcu), &error_abort);

    if (machine->firmware) {
        if (!nes6502_load_firmware(&ams->mcu.cpu, &ams->mcu, &ams->mcu.flash, machine->firmware)) {
            exit(1);
        }
    }
}

static void xiaobawang_machine_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "xiaobawang game machine";
    mc->init = xiaobawang_init;
    mc->is_default = true;
}

// DEFINE_MACHINE("xiaobawang", xiaobawang_machine_init)

static const TypeInfo arduino_machine_types[] = {
    {
        .name          = MACHINE_TYPE_NAME("xiaobawang-v1"),
        .parent        = TYPE_XIAOBAWANG_MACHINE,
        .class_init    = arduino_mega2560_class_init,
    }, {
        .name           = TYPE_XIAOBAWANG_MACHINE,
        .parent         = TYPE_MACHINE,
        .instance_size  = sizeof(XiaoBaWangMachineState),
        .class_size     = sizeof(XiaoBaWangMachineClass),
        .class_init     = xiaobawang_machine_init,
        .abstract       = true,
    }
};

DEFINE_TYPES(arduino_machine_types)
