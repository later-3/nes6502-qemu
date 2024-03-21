/*
 * nes loader helpers
 *
 * Copyright (c) 2019-2020 
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/datadir.h"
#include "hw/loader.h"
#include "elf.h"
#include "boot.h"
#include "qemu/error-report.h"
#include "sysemu/reset.h"

/* program memory */
#define RAM_ADDR 0x0
#define FIRST_CODE_OFFSET 0x8000
#define SECOND_CODE_OFFSET 0xC000
#define PRG_BLOGK_SIZE 0x4000

typedef uint8_t byte;
typedef uint16_t word;
typedef uint32_t dword;
typedef uint64_t qword;

typedef struct {
    char signature[4];
    byte prg_block_count;
    byte chr_block_count;
    word rom_type;
    byte reserved[8];
} ines_header;

static ines_header fce_rom_header;
static char rom[1048576];
static void romread(char *rom, void *buf, int size)
{
    static int off = 0;
    memcpy(buf, rom + off, size);
    off += size;
}

byte mmc_id;

static void mmc_append_chr_rom_page(byte *source, int size, int page_number)
{
    address_space_write(&address_space_memory, 0x18000 + page_number * 0x2000, MEMTXATTRS_UNSPECIFIED, source, 0x2000);
}

static int fce_load_rom(char *rom, NesMcuState *s)
{
    romread(rom, &fce_rom_header, sizeof(fce_rom_header));

    if (memcmp(fce_rom_header.signature, "NES\x1A", 4)) {
        return -1;
    }
    mmc_id = ((fce_rom_header.rom_type & 0xF0) >> 4);

    int prg_size = fce_rom_header.prg_block_count * 0x4000;
    static byte buf[1048576];
    romread(rom, buf, prg_size);

    if (mmc_id == 0 || mmc_id == 3) {

        if (fce_rom_header.prg_block_count == 1) {
            // mmc_copy(0x8000, buf, 0x4000);
            // mmc_copy(0xC000, buf, 0x4000);
            address_space_write(&address_space_memory, FIRST_CODE_OFFSET, MEMTXATTRS_UNSPECIFIED, buf, PRG_BLOGK_SIZE);
            address_space_write(&address_space_memory, 0xC000, MEMTXATTRS_UNSPECIFIED, buf, PRG_BLOGK_SIZE);
        }
        else {
            // mmc_copy(0x8000, buf, 0x8000);
            address_space_write(&address_space_memory, 0x8000, MEMTXATTRS_UNSPECIFIED, buf, 0x8000);
        }

    }
    else {
        return -1;
    }

    // Copying CHR pages into MMC and PPU
    int i;
    for (i = 0; i < fce_rom_header.chr_block_count; i++) {
        romread(rom, buf, 0x2000);
        mmc_append_chr_rom_page(buf, WORK_RAM_SIZE, s->mmc_chr_pages_number);
        s->mmc_chr_pages_number++;

        if (i == 0) {
            ppu_copy(0x0000, buf, 0x2000);
        }
    }

    return 0;
}

static void do_cpu_reset(void *opaque)
{
    NES6502CPU *cpu = opaque;
    CPUState *cs = CPU(cpu);
    uint16_t value;
    address_space_read(&address_space_memory, 0xFFFC, MEMTXATTRS_UNSPECIFIED,
                             &value, 2);
    cpu_set_pc(cs, value);
}

bool nes6502_load_firmware(NES6502CPU *cpu, NesMcuState *s,
                       MemoryRegion *program_mr, const char *firmware)
{
    g_autofree char *filename = NULL;
    // int bytes_loaded;
    // uint64_t entry;
    // uint32_t e_flags;

    filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, firmware);
    if (filename == NULL) {
        error_report("Unable to find %s", firmware);
        return false;
    }

    FILE *fp = fopen(firmware, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Open rom file failed.\n");
        exit(1);
    }
    int nread = fread(rom, sizeof(rom), 1, fp);
    if (nread == 0 && ferror(fp)) {
        fprintf(stderr, "Read rom file failed.\n");
        exit(1);
    }
    if (fce_load_rom(rom, s) != 0)
    {
        fprintf(stderr, "Invalid or unsupported rom.\n");
        exit(1);
    }

    int val = fce_rom_header.rom_type & 1;
    address_space_write(&address_space_memory, 0x2000 + 8, MEMTXATTRS_UNSPECIFIED, &val, 4);

    qemu_register_reset(do_cpu_reset, cpu);
    // signal(SIGINT, do_exit);
    return true;
}
