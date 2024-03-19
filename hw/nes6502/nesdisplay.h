/*
 * NeXT Cube
 *
 * Copyright (c) 2011 Bryce Lanham
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef NES6502_CUBE_H
#define NES6502_CUBE_H

#define TYPE_NES6502FB "nes-fb"
OBJECT_DECLARE_SIMPLE_TYPE(NES6502FbState, NES6502FB)

struct NES6502FbState {
    SysBusDevice parent_obj;

    MemoryRegion fb_mr;
    MemoryRegion mr;
    MemoryRegionSection fbsection;
    QemuConsole *con;

    uint32_t cols;
    uint32_t rows;
    int invalidate;
};


#endif /* NES6502_CUBE_H */
