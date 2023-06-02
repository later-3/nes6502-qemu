gen = [
  decodetree.process('insn.decode', extra_args: [ '--decode', 'decode_insn',
                                                  '--varinsnwidth', '16' ])
]

nes6502_ss = ss.source_set()
nes6502_softmmu_ss = ss.source_set()

nes6502_ss.add(gen)
nes6502_ss.add(files(
  'translate.c',
  'helper.c',
  'cpu.c',
  'gdbstub.c',
  'disas.c'))

nes6502_softmmu_ss.add(files('machine.c'))

target_arch += {'nes6502': nes6502_ss}
target_softmmu_arch += {'nes6502': nes6502_softmmu_ss}
