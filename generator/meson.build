fs = import('fs')

template_sources = [
  'layer/armv4.cpp',
  'layer/debug.cpp',
  'layer/debug.hpp',
  'layer/helpers.hpp',
  'layer/state.hpp',
  'layer/meson.build',

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
