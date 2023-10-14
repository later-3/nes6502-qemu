
#include "nesppu.h"

#ifndef DEBUG_IMX_UART
#define DEBUG_IMX_UART 0
#endif

byte ppu_sprite_palette[4][4];
bool ppu_2007_first_read;
byte ppu_addr_latch;
byte ppu_latch;
bool ppu_sprite_hit_occured = false;

byte PPU_SPRRAM[0x100];
byte PPU_RAM[0x4000];

// PPUSTATUS Functions

// inline bool ppu_sprite_overflow()                                   { return common_bit_set(ppu.PPUSTATUS, 5); }
// inline bool ppu_sprite_0_hit()                                      { return common_bit_set(ppu.PPUSTATUS, 6); }
// inline bool ppu_in_vblank()                                         { return common_bit_set(ppu.PPUSTATUS, 7); }

// inline void ppu_set_sprite_overflow(bool yesno)                     { common_modify_bitb(&ppu.PPUSTATUS, 5, yesno); }
static inline void ppu_set_sprite_0_hit(PPUState *ppu, bool yesno)          { common_modify_bitb(&ppu->PPUSTATUS, 6, yesno); }
static inline void ppu_set_in_vblank(PPUState *ppu, bool yesno)            { common_modify_bitb(&ppu->PPUSTATUS, 7, yesno); }


static uint64_t ppu_read(void *opaque, hwaddr offset,
                                unsigned size)
{
    PPUState *ppu = opaque;
    word address = offset + 0x2000;
    ppu->PPUADDR &= 0x3FFF;
    switch (address & 7) {
        case 2:
        {
            byte value = ppu->PPUSTATUS;
            ppu_set_in_vblank(ppu, false);
            ppu_set_sprite_0_hit(ppu, false);
            ppu->scroll_received_x = 0;
            ppu->PPUSCROLL = 0;
            ppu->addr_received_high_byte = 0;
            ppu_latch = value;
            ppu_addr_latch = 0;
            ppu_2007_first_read = true;
            return value;
        }
        case 4: return ppu_latch = PPU_SPRRAM[ppu->OAMADDR];
        case 7:
        {
            byte data = 0;
            
            if (ppu->PPUADDR < 0x3F00) {
                // data = ppu_latch = ppu_ram_read(ppu->PPUADDR);
            }
            else {
                // data = ppu_ram_read(ppu->PPUADDR);
                ppu_latch = 0;
            }
            
            if (ppu_2007_first_read) {
                ppu_2007_first_read = false;
            }
            else {
                // ppu->PPUADDR += ppu_vram_address_increment();
            }
            return data;
        }
        default:
            return 0xFF;
    }
    return 0;
}

static word ppu_get_real_ram_address(word address)
{
    if (address < 0x2000) {
        return address;
    }
    else if (address < 0x3F00) {
        if (address < 0x3000) {
            return address;
        }
        else {
            return address;// - 0x1000;
        }
    }
    else if (address < 0x4000) {
        address = 0x3F00 | (address & 0x1F);
        if (address == 0x3F10 || address == 0x3F14 || address == 0x3F18 || address == 0x3F1C)
            return address - 0x10;
        else
            return address;
    }
    return 0xFFFF;
}

static void ppu_ram_write(word address, byte data)
{
    PPU_RAM[ppu_get_real_ram_address(address)] = data;
}

static void ppu_write(void *opaque, hwaddr offset,
                             uint64_t data, unsigned size)
{
    PPUState *ppu = opaque;
    word address = offset + 0x2000;
    address &= 7;
    ppu_latch = data;
    ppu->PPUADDR &= 0x3FFF;
    switch(address) {
        case 0: if (ppu->ready) ppu->PPUCTRL = data; break;
        case 1: if (ppu->ready) ppu->PPUMASK = data; break;
        case 3: ppu->OAMADDR = data; break;
        case 4: PPU_SPRRAM[ppu->OAMADDR++] = data; break;
        case 5:
        {
            if (ppu->scroll_received_x)
                ppu->PPUSCROLL_Y = data;
            else
                ppu->PPUSCROLL_X = data;

            ppu->scroll_received_x ^= 1;
            break;
        }
        case 6:
        {
            if (!ppu->ready)
                return;

            if (ppu->addr_received_high_byte)
                ppu->PPUADDR = (ppu_addr_latch << 8) + data;
            else
                ppu_addr_latch = data;

            ppu->addr_received_high_byte ^= 1;
            ppu_2007_first_read = true;
            break;
        }
        case 7:
        {
            if (ppu->PPUADDR > 0x1FFF || ppu->PPUADDR < 0x4000) {
                ppu_ram_write(ppu->PPUADDR ^ ppu->mirroring_xor, data);
                ppu_ram_write(ppu->PPUADDR, data);
            }
            else {
                ppu_ram_write(ppu->PPUADDR, data);
            }
        }
    }
    ppu_latch = data;
}

static const struct MemoryRegionOps ppu_ops = {
    .read = ppu_read,
    .write = ppu_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
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
