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

#define TYPE_NES6502_MCU     "nesmcu"
#define TYPE_ATMEGA168_MCU  "ATmega168"
#define TYPE_ATMEGA328_MCU  "ATmega328"
#define TYPE_ATMEGA1280_MCU "ATmega1280"
#define TYPE_ATMEGA2560_MCU "ATmega2560"

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

    AVRCPU cpu;
    MemoryRegion flash;
    MemoryRegion eeprom;
    MemoryRegion work_ram;
    MemoryRegion chr_ram;
    DeviceState *io;
    AVRMaskState pwr[POWER_MAX];
    AVRUsartState usart[USART_MAX];
    AVRTimer16State timer[TIMER_MAX];
    uint64_t xtal_freq_hz;

    int mmc_prg_pages_number;
    int mmc_chr_pages_number;
    struct PPUState ppu;
};

#endif /* HW_AVR_ATMEGA_H */
