/*
 * QEMU ATmega MCU
 *
 * Copyright (c) 2019-2020 Philippe Mathieu-Daudé
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_AVR_ATMEGA_H
#define HW_AVR_ATMEGA_H

#include "hw/char/avr_usart.h"
#include "hw/timer/avr_timer16.h"
#include "hw/misc/avr_power.h"
#include "target/nes6502/cpu.h"
#include "qom/object.h"
#include "qemu/units.h"
#include "nesppu.h"
#include "nespsg.h"
#include "hw/timer/renesas_tmr.h"
#include "nesdisplay.h"
#define TYPE_NES6502_MCU     "nesmcu"

typedef struct NesMcuState NesMcuState;
DECLARE_INSTANCE_CHECKER(NesMcuState, NES6502_MCU,
                         TYPE_NES6502_MCU)

#define POWER_MAX 2
#define USART_MAX 4
#define TIMER_MAX 6
#define GPIO_MAX 12

#define WORK_RAM_SIZE 64 * KiB

struct NesMcuState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    NES6502CPU cpu;
    MemoryRegion flash;
    MemoryRegion eeprom;
    MemoryRegion cpu_ram;
    MemoryRegion chr_ram;
    MemoryRegion mmc_ram;
    MemoryRegion cpu_ram_alias;
    DeviceState *io;
    AVRMaskState pwr[POWER_MAX];
    AVRUsartState usart[USART_MAX];
    AVRTimer16State timer[TIMER_MAX];
    uint64_t xtal_freq_hz;
    RTMRState tmr;
    int mmc_prg_pages_number;
    int mmc_chr_pages_number;
    struct PPUState ppu;
    struct NesKBDState kbd;
    struct NES6502FbState nesfb;
};

#endif /* HW_AVR_ATMEGA_H */
