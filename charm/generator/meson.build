fs = import('fs')

template_sources = [
  'src/armv4.cpp',
  'src/debug.cpp',
  'src/debug.hpp',
  'src/helpers.hpp',
  'src/state.hpp',
  'src/meson.build',

  'templates/code.hpp.tl',
  'templates/code.cpp.tl',
  'templates/memory.hpp.tl',
  'templates/memory.cpp.tl',
]

foreach f : template_sources
  configure_file(
    input: f,
    output: fs.name(f),
    copy: true,
  )
endforeach
