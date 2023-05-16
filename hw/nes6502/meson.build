nes6502_ss = ss.source_set()
nes6502_ss.add(files('boot.c'))
nes6502_ss.add(when: 'CONFIG_NES6502_ATMEGA_MCU', if_true: files('atmega.c'))
nes6502_ss.add(when: 'CONFIG_ARDUINO', if_true: files('arduino.c'))

hw_arch += {'nes6502': nes6502_ss}
