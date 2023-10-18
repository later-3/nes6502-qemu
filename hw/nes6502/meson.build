nes6502_ss = ss.source_set()
nes6502_ss.add(files('boot.c'))
nes6502_ss.add(when: 'CONFIG_NES6502_MCU', if_true: files('nesmcu.c', 'nesppu.c', 'nespsg.c', 'common.c', 'nesdisplay.c'))
nes6502_ss.add(when: 'CONFIG_XIAOBAWANG', if_true: files('xiaobawang.c'))

hw_arch += {'nes6502': nes6502_ss}
