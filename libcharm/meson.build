libcharm = library(
  'charm',

  include_directories: [
    'include',
    'external/ELFIO',
  ],

  sources: [
    'src/arm.cpp',
    'src/emulator.cpp',
    'src/recomp.cpp',
    'src/recomp_analysis.cpp',
    'src/recomp_emit.cpp',
  ],

  dependencies: [liblayer_dep],
)

libcharm_dep = declare_dependency(
  link_with: libcharm,
  include_directories: [
    'include',
    'external/ELFIO',
  ],
)
