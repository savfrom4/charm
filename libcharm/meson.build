libcharm = library(
  'charm',

  include_directories: [
    'include',
    'external/ELFIO',
  ],

  sources: files(
    'src/arm.cpp',
    'src/recomp.cpp',
    'src/recomp_analysis.cpp',
    'src/recomp_emit.cpp',
    'src/recomp_emit_arm.cpp',
  ),
)

libcharm_dep = declare_dependency(
  link_with: libcharm,
  include_directories: [
    'include',
    'external/ELFIO',
  ],
)
