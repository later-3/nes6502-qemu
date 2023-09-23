/*
 * QEMU ATmega MCU
 *
 * Copyright (c) 2019-2020 Philippe Mathieu-Daudé
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/misc/unimp.h"
#include "nesmcu.h"

struct NesMcuClass {
    /*< private >*/
    SysBusDeviceClass parent_class;
    /*< public >*/
    const char *uc_name;
    const char *cpu_type;
    size_t flash_size;
    size_t eeprom_size;
    size_t cpu_ram_size;
    size_t io_size;
    size_t gpio_count;
    size_t adc_count;
    const uint8_t *irq;
};
typedef struct NesMcuClass NesMcuClass;

DECLARE_CLASS_CHECKERS(NesMcuClass, NES6502_MCU,
                       TYPE_NES6502_MCU)
#define MMC_MAX_PAGE_COUNT 256
static void atmega_realize(DeviceState *dev, Error **errp)
{
    NesMcuState *s = NES6502_MCU(dev);
    const NesMcuClass *mc = NES6502_MCU_GET_CLASS(dev);
    assert(mc->io_size <= 0x200);

    if (!s->xtal_freq_hz) {
        error_setg(errp, "\"xtal-frequency-hz\" property must be provided.");
        return;
    }

    /* CPU */
    object_initialize_child(OBJECT(dev), "cpu", &s->cpu, mc->cpu_type);
    qdev_realize(DEVICE(&s->cpu), NULL, &error_abort);
    // cpudev = DEVICE(&s->cpu);

    /* CPU RAM */
    memory_region_init_ram(&s->cpu_ram, OBJECT(dev), "cpu_ram", mc->cpu_ram_size, &error_abort);
    memory_region_add_subregion(get_system_memory(), RAM_ADDR, &s->cpu_ram);

    /* PPU IO*/
    /* PPU controller*/
    object_initialize_child(OBJECT(dev), "ppu", &s->ppu, TYPE_NES_PPU);
    sysbus_realize(SYS_BUS_DEVICE(&s->ppu), &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ppu), 0, 0x2000);

    /* PSG IO*/
    object_initialize_child(OBJECT(dev), "kbd", &s->kbd, TYPE_NES_KBD);
    sysbus_realize(SYS_BUS_DEVICE(&s->kbd), &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->kbd), 0, 0x4000);

    /* mirror cpu ram*/
    memory_region_init_alias(&s->cpu_ram_alias, OBJECT(dev),
                             "nes.cpu_ram_alias", &s->cpu_ram, 0,
                             0x2000);
    memory_region_add_subregion(get_system_memory(), 0x6000,
                                &s->cpu_ram_alias);

    int val = 0x123456;
    address_space_write(&address_space_memory, 0x1000,
                    MEMTXATTRS_UNSPECIFIED, &val,
                    4);

    /* MMC */
    memory_region_init_ram(&s->mmc_ram, OBJECT(dev), "mmc", 0x10000, &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x8000, &s->mmc_ram);


    /* CHR RAM*/
    memory_region_init_ram(&s->chr_ram, OBJECT(dev), "chr_ram", MMC_MAX_PAGE_COUNT * 0x2000, &error_abort);
    memory_region_add_subregion(get_system_memory(), 0x8000, &s->chr_ram);


}

static Property atmega_props[] = {
    DEFINE_PROP_UINT64("xtal-frequency-hz", NesMcuState,
                       xtal_freq_hz, 0),
    DEFINE_PROP_END_OF_LIST()
};

static void nes6502_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = atmega_realize;
    device_class_set_props(dc, atmega_props);
    /* Reason: Mapped at fixed location on the system bus */
    dc->user_creatable = false;

    NesMcuClass *nes6502_cpu_class = NES6502_MCU_CLASS(oc);
    nes6502_cpu_class->cpu_type = TYPE_NES6502_CPU;
    nes6502_cpu_class->cpu_ram_size = 0x2000;
}

static const TypeInfo nes6502_mcu_types[] = {
    {
        .name           = TYPE_NES6502_MCU,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(NesMcuState),
        .class_size     = sizeof(NesMcuClass),
        .class_init     = nes6502_class_init,
    }
};

DEFINE_TYPES(nes6502_mcu_types)
