
/*
 * This is admittedly hackish, but works well enough for basic input. Mouse
 * support will be added once we can boot something that needs the mouse.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/sysbus.h"
#include "nespsg.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"


static uint8_t  prev_write;
static int p = 10;
/*
inline byte psg_io_read(word address)
{
    // Joystick 1
    if (address == 0x4016) {
        if (p++ < 9) {
            return nes_key_state(p);
        }
    }
    return 0;
}

inline void psg_io_write(word address, byte data)
{
    if (address == 0x4016) {
        if ((data & 1) == 0 && prev_write == 1) {
            // strobe
            p = 0;
        }
    }
    prev_write = data & 1;
}
*/
static int key_arr[10];
static int key_index;

static int nes_psg_query_key(int ch)
{
    for (int i = 0; i < 9; i++) {
        // printf("ch %d, val %d\n", ch, key_arr[i]);
        if (key_arr[i] == ch) {
            return 1;
        }
    }
    return 0;
}

static int nes_psg_key_state(int b)
{
    printf("query key %d\n", b);
    switch (b) // w 17 a 30 d 32 s 31 i 23  j 36 k 37
    {
        case 0: // On / Off
            return 1;
        case 1: // A
            return nes_psg_query_key(37);
        case 2: // B
            return nes_psg_query_key(36);
        case 3: // SELECT
            return nes_psg_query_key(22);
        case 4: // START
            return nes_psg_query_key(23);
        case 5: // UP
            return nes_psg_query_key(17);
        case 6: // DOWN
            return nes_psg_query_key(31);
        case 7: // LEFT
            return nes_psg_query_key(30);
        case 8: // RIGHT
            return nes_psg_query_key(32);
        default:
            return 1;
    }
}

static uint64_t kbd_readfn(void *opaque, hwaddr addr, unsigned size)
{
    if (addr == 0x16) {
        if (p++ < 9) {
            return nes_psg_key_state(p);
        }
    }
    return 0;
}

static uint8_t cpu_ram_read(uint16_t addr)
{
    uint8_t value;
    address_space_read(&address_space_memory, addr, MEMTXATTRS_UNSPECIFIED,
                             &value, 1);
    return value;
}


static void kbd_writefn(void *opaque, hwaddr addr, uint64_t value,
                        unsigned size)
{
    if (addr == 0x14) {
        uint8_t val;
        for (int i = 0; i < 256; i++) {
            val = cpu_ram_read((0x100 * value) + i);
            address_space_write(&address_space_memory, 0x2000 + 9, MEMTXATTRS_UNSPECIFIED, &val, 4);
        }
    }

    if (addr == 0x16) {
        if ((value & 1) == 0 && prev_write == 1) {
            // strobe
            p = 0;
        }
    }
    prev_write = value & 1;
}

static const MemoryRegionOps kbd_ops = {
    .read = kbd_readfn,
    .write = kbd_writefn,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void nextkbd_event(void *opaque, int ch)
{
    // printf("ch %d\n", ch);// w 17 a 30 d 32 s 31 i 23  j 36 k 37 u 22
    if (key_index > 9) {
        memset(key_arr, 0, sizeof(*key_arr));
        key_index = 0;
    }

    switch (ch) {
        case 17: //w
            key_arr[key_index++] = 17;
            break;
        case 30: //a
            key_arr[key_index++] = 30;
            break;
        case 32: //d
            key_arr[key_index++] = 32;
            break;
        case 31: //s
            key_arr[key_index++] = 31;
            break;
        case 23: //i
            key_arr[key_index++] = 23;
            break;
        case 36: //j
            key_arr[key_index++] = 36;
            break;
        case 37: //k
            key_arr[key_index++] = 37;
            break;
        case 22: //u
            key_arr[key_index++] = 22;
            break;
    }
}

static void nextkbd_reset(DeviceState *dev)
{
    NesKBDState *nks = NES_KBD(dev);

    memset(&nks->queue, 0, sizeof(KBDQueue));
    nks->shift = 0;
}

static void nextkbd_realize(DeviceState *dev, Error **errp)
{
    NesKBDState *s = NES_KBD(dev);

    memory_region_init_io(&s->mr, OBJECT(dev), &kbd_ops, s, "nes.kbd", 0x2000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mr);

    qemu_add_kbd_event_handler(nextkbd_event, s);
}

static const VMStateDescription nextkbd_vmstate = {
    .name = TYPE_NES_KBD,
    .unmigratable = 1,    /* TODO: Implement this when m68k CPU is migratable */
};

static void nextkbd_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd = &nextkbd_vmstate;
    dc->realize = nextkbd_realize;
    dc->reset = nextkbd_reset;
}

static const TypeInfo nextkbd_info = {
    .name          = TYPE_NES_KBD,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NesKBDState),
    .class_init    = nextkbd_class_init,
};

static void nextkbd_register_types(void)
{
    type_register_static(&nextkbd_info);
}

type_init(nextkbd_register_types)
