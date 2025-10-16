fs = import('fs')

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
