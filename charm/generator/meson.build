fs = import('fs')

template_sources = [
  'include/helpers.hpp',
  'include/debug.hpp',
  'include/state.hpp',

  'src/armv4.cpp',
  'src/debug.cpp',
  'src/meson.build',
  'src/meson_options.txt',

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
