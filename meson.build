project('charm', 'cpp')

subdir('charm')
subdir('debugger')
subdir('generator')

executable(
  'charm',

  include_directories: [
    'include',
    'external/ELFIO',
  ],

  sources: files(
    'src/arm.cpp',
    'src/main.cpp',
    'src/recomp.cpp',
    'src/recomp_analysis.cpp',
    'src/recomp_emit.cpp',
    'src/recomp_emit_arm.cpp',
  ),
)
