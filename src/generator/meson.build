fs = import('fs')

template_sources = [
  'templates/code.hpp.tl',
  'templates/code.cpp.tl',
]

foreach f : template_sources
  configure_file(
    input: f,
    output: fs.name(f),
    copy: true,
  )
endforeach
