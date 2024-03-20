/*
 * NeXT Cube/Station Framebuffer Emulation
 *
 * Copyright (c) 2011 Bryce Lanham
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "ui/console.h"
#include "hw/loader.h"
#include "hw/display/framebuffer.h"
#include "ui/pixel_ops.h"
#include "qom/object.h"
#include "nesdisplay.h"
#include "hal.h"


static const pal palette[64] = {
	{ 0x80, 0x80, 0x80 },
	{ 0x00, 0x00, 0xBB },
	{ 0x37, 0x00, 0xBF },
	{ 0x84, 0x00, 0xA6 },
	{ 0xBB, 0x00, 0x6A },
	{ 0xB7, 0x00, 0x1E },
	{ 0xB3, 0x00, 0x00 },
	{ 0x91, 0x26, 0x00 },
	{ 0x7B, 0x2B, 0x00 },
	{ 0x00, 0x3E, 0x00 },
	{ 0x00, 0x48, 0x0D },
	{ 0x00, 0x3C, 0x22 },
	{ 0x00, 0x2F, 0x66 },
	{ 0x00, 0x00, 0x00 },
	{ 0x05, 0x05, 0x05 },
	{ 0x05, 0x05, 0x05 },
	{ 0xC8, 0xC8, 0xC8 },
	{ 0x00, 0x59, 0xFF },
	{ 0x44, 0x3C, 0xFF },
	{ 0xB7, 0x33, 0xCC },
	{ 0xFF, 0x33, 0xAA },
	{ 0xFF, 0x37, 0x5E },
	{ 0xFF, 0x37, 0x1A },
	{ 0xD5, 0x4B, 0x00 },
	{ 0xC4, 0x62, 0x00 },
	{ 0x3C, 0x7B, 0x00 },
	{ 0x1E, 0x84, 0x15 },
	{ 0x00, 0x95, 0x66 },
	{ 0x00, 0x84, 0xC4 },
	{ 0x11, 0x11, 0x11 },
	{ 0x09, 0x09, 0x09 },
	{ 0x09, 0x09, 0x09 },
	{ 0xFF, 0xFF, 0xFF },
	{ 0x00, 0x95, 0xFF },
	{ 0x6F, 0x84, 0xFF },
	{ 0xD5, 0x6F, 0xFF },
	{ 0xFF, 0x77, 0xCC },
	{ 0xFF, 0x6F, 0x99 },
	{ 0xFF, 0x7B, 0x59 },
	{ 0xFF, 0x91, 0x5F },
	{ 0xFF, 0xA2, 0x33 },
	{ 0xA6, 0xBF, 0x00 },
	{ 0x51, 0xD9, 0x6A },
	{ 0x4D, 0xD5, 0xAE },
	{ 0x00, 0xD9, 0xFF },
	{ 0x66, 0x66, 0x66 },
	{ 0x0D, 0x0D, 0x0D },
	{ 0x0D, 0x0D, 0x0D },
	{ 0xFF, 0xFF, 0xFF },
	{ 0x84, 0xBF, 0xFF },
	{ 0xBB, 0xBB, 0xFF },
	{ 0xD0, 0xBB, 0xFF },
	{ 0xFF, 0xBF, 0xEA },
	{ 0xFF, 0xBF, 0xCC },
	{ 0xFF, 0xC4, 0xB7 },
	{ 0xFF, 0xCC, 0xAE },
	{ 0xFF, 0xD9, 0xA2 },
	{ 0xCC, 0xE1, 0x99 },
	{ 0xAE, 0xEE, 0xB7 },
	{ 0xAA, 0xF7, 0xEE },
	{ 0xB3, 0xEE, 0xFF },
	{ 0xDD, 0xDD, 0xDD },
	{ 0x11, 0x11, 0x11 },
	{ 0x11, 0x11, 0x11 }
};


unsigned int color_map[64];

static NESFB_VERTEX vtx[1000000];
int vtx_sz = 0;

PixelBuf bg, bbg, fg;
static int color_index;
static void nextfb_draw_line(void *opaque, uint8_t *d, const uint8_t *s,
                             int width, int pitch)
{
    NES6502FbState *nfbstate = NES6502FB(opaque);
    // static const uint32_t pal[4] = {
    //     0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000
    // };
    unsigned int *buf = (unsigned int *)d;
    int x;
    int y;
    int pixel_index;
    for (int i = 0; i < vtx_sz; i++) {
        x = vtx[i].x;
        y = (nfbstate->cols * 2) * vtx[i].y;
        pixel_index = (x + y);
        if (pixel_index > (512 * 480)) {
            printf("wrong index %d\n", pixel_index);
            continue;
        }
        buf[pixel_index] = vtx[i].color;
    }

    vtx_sz = 0;
}

static void nextfb_update(void *opaque)
{
    NES6502FbState *s = NES6502FB(opaque);
    int dest_width = 4;
    int src_width;
    int first = 0;
    int last  = 0;
    DisplaySurface *surface = qemu_console_surface(s->con);

    src_width = s->cols * 2;
    dest_width = s->cols * 2;

    if (s->invalidate) {
        framebuffer_update_memory_section(&s->fbsection, &s->fb_mr, 0,
                                          s->rows * 2, src_width);
        s->invalidate = 0;
    }

    framebuffer_update_display(surface, &s->fbsection, s->cols * 2, s->rows * 2,
                               src_width, dest_width, 0, 1, nextfb_draw_line,
                               s, &first, &last);

    dpy_gfx_update(s->con, 0, 0, s->cols * 2, s->rows * 2);
}

static void nextfb_invalidate(void *opaque)
{
    NES6502FbState *s = NES6502FB(opaque);
    s->invalidate = 1;
}

static const GraphicHwOps nextfb_ops = {
    .invalidate  = nextfb_invalidate,
    // .gfx_update  = nextfb_update,
};

static void nesfb_clear_to_color(int c)
{
    color_index = c;
}

static uint64_t nesfb_read(void *opaque, hwaddr addr, unsigned size)
{
    return 0;
}

void nes_flip_display(void *opaque)
{
    nextfb_update(opaque);
}

/* Flush the pixel buffer */
static void nes_flush_buf(PixelBuf *buf) {
    int i;
    unsigned int  c;
    for (i = 0; i < buf->size; i ++) {
        Pixel *p = &buf->buf[i];
        int x = p->x, y = p->y;
        c = color_map[p->c];

        vtx[vtx_sz].x = x*2; vtx[vtx_sz].y = y*2;
        vtx[vtx_sz ++].color = c;
        vtx[vtx_sz].x = x*2+1; vtx[vtx_sz].y = y*2;
        vtx[vtx_sz ++].color = c;
        vtx[vtx_sz].x = x*2; vtx[vtx_sz].y = y*2+1;
        vtx[vtx_sz ++].color = c;
        vtx[vtx_sz].x = x*2+1; vtx[vtx_sz].y = y*2+1;
        vtx[vtx_sz ++].color = c;
    }
}

static void nesfb_write(void *opaque, hwaddr addr, uint64_t value,
                        unsigned size)
{
    int offset = addr >> 2;
    switch (offset) {
    case NESFB_SET_BG_COLOR:
        nesfb_clear_to_color(value);
        break;
    case NFSFB_FLUSH_BBG:
        nes_flush_buf(&bbg);
        break;
    case NFSFB_FLUSH_BG:
        nes_flush_buf(&bg);
        break;
    case NFSFB_FLUSH_FG:
        nes_flush_buf(&fg);
        break;
    case NFSFB_FLIP_DISPLAY:
        nes_flip_display(opaque);
        break;
    default:
        g_assert_not_reached();
    }
}

static const MemoryRegionOps nesfb_ops = {
    .read = nesfb_read,
    .write = nesfb_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void nextfb_realize(DeviceState *dev, Error **errp)
{
    NES6502FbState *s = NES6502FB(dev);

    memory_region_init_ram(&s->fb_mr, OBJECT(dev), "next-video", 0x1CB100,
                           &error_fatal);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->fb_mr);

    s->invalidate = 1;
    s->cols = SCREEN_WIDTH;
    s->rows = SCREEN_HEIGHT;

    s->con = graphic_console_init(dev, 0, &nextfb_ops, s);
    qemu_console_resize(s->con, s->cols * 2, s->rows * 2);

    memory_region_init_io(&s->mr, OBJECT(dev), &nesfb_ops, s, "nes.fb.op", 0x20000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mr);

    for (int i = 0; i < 64; i ++) {
        pal color = palette[i];
        color_map[i] = rgb_to_pixel32(color.r, color.g, color.b);
    }
}

static void nextfb_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_DISPLAY, dc->categories);
    dc->realize = nextfb_realize;

    /* Note: This device does not have any state that we have to reset or migrate */
}

static const TypeInfo nextfb_info = {
    .name          = TYPE_NES6502FB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(NES6502FbState),
    .class_init    = nextfb_class_init,
};

static void nextfb_register_types(void)
{
    type_register_static(&nextfb_info);
}

type_init(nextfb_register_types)
