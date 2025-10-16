fs = import('fs')

executable(
  'recompiler',

  include_directories: include_directories,

  sources: files(
    'main.cpp',
    'recomp.cpp',
    'recomp_analysis.cpp',
    'recomp_emit.cpp',
    'recomp_emit_arm.cpp',
  ),

  dependencies: [libisa_dep],
)

template_sources = [
  'templates/code.hpp.tl',
  'templates/code.cpp.tl',
  'templates/data.cpp.tl',
  'templates/meson.build',
  'templates/meson_options.txt',
]

foreach f : template_sources
  configure_file(
    input: f,
    output: fs.name(f),
    copy: true,
  )
endforeach
