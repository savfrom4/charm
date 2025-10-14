subdir('generator')

include_directories = [
  '../include',
  '../external/ELFIO',
]

libisa_dep = declare_dependency(
  include_directories: include_directories,

  sources: files(
    'isa/arm.cpp',
  ),
)

libruntime_dep = declare_dependency(
  include_directories: include_directories,

  sources: files(
    'runtime/armv4.cpp',
    'runtime/debug.cpp',
    'runtime/memory.cpp',
  ),
)

executable(
  'recompiler',

  include_directories: include_directories,

  sources: files(
    'recompiler/main.cpp',
    'recompiler/recomp.cpp',
    'recompiler/recomp_analysis.cpp',
    'recompiler/recomp_emit.cpp',
    'recompiler/recomp_emit_arm.cpp',
  ),

  dependencies: [libisa_dep],
)

executable(
  'debugger',

  include_directories: include_directories,

  sources: files(
    'debugger/main.cpp',
  ),
)
