/*
 * IMX31 UARTS
 *
 * Copyright (c) 2008 OKL
 * Originally Written by Hans Jiang
 * Copyright (c) 2011 NICTA Pty Ltd.
 * Updated by Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * This is a `bare-bones' implementation of the IMX series serial ports.
 * TODO:
 *  -- implement FIFOs.  The real hardware has 32 word transmit
 *                       and receive FIFOs; we currently use a 1-char buffer
 *  -- implement DMA
 *  -- implement BAUD-rate and modem lines, for when the backend
 *     is a real serial device.
 */
#include "nesppu.h"

#ifndef DEBUG_IMX_UART
#define DEBUG_IMX_UART 0
#endif

#define DPRINTF(fmt, args...) \
    do { \
        if (DEBUG_IMX_UART) { \
            fprintf(stderr, "[%s]%s: " fmt , TYPE_NES_PPU, \
                                             __func__, ##args); \
        } \
    } while (0)



static uint64_t ppu_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    return 0;
}

static void ppu_write(void *opaque, hwaddr offset,
                             uint64_t value, unsigned size)
{
    printf("ppu write\n");
}

static const struct MemoryRegionOps ppu_ops = {
    .read = ppu_read,
    .write = ppu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ppu_realize(DeviceState *dev, Error **errp)
{

}

static void ppu_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    PPUState *s = NES_PPU(obj);

    memory_region_init_io(&s->iomem, obj, &ppu_ops, s,
                          TYPE_NES_PPU, 0x2000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static void ppu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ppu_realize;
    dc->desc = "xiaobawang nes ppu";
}
static const TypeInfo nes_ppu_info = {
    .name           = TYPE_NES_PPU,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(PPUState),
    .instance_init  = ppu_init,
    .class_init     = ppu_class_init,
};

static void nes_ppu_register_types(void)
{
    type_register_static(&nes_ppu_info);
}

type_init(nes_ppu_register_types)
