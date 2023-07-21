#ifndef NES_PPU_H
#define NES_PPU_H

#include "qemu/osdep.h"
#include "hw/char/imx_serial.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"

#define TYPE_NES_PPU "nes ppu" 
OBJECT_DECLARE_SIMPLE_TYPE(PPUState, NES_PPU)


struct PPUState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion iomem;
};


#endif