template_sources = [
  'template.code.cpp',
  'template.code.hpp',
  'template.meson.build',
]

foreach f : template_sources
  configure_file(
    input: f,
    output: f,
    copy: true,
  )
endforeach
