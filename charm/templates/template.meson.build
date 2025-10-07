project('output', 'cpp', default_options: ['b_lto=true'])
add_project_arguments('-Wno-unused-label', language: 'cpp')

subdir('liblayer')

include_directories = ['.']
dependencies = [liblayer_dep]

sources = files(
  'code.cpp',
  'data.cpp',
)

if get_option('debugging')
  add_project_arguments('-DLAYER_DEBUG', language: 'cpp')
endif

if get_option('library')
  liboutput_dep = library(
    'output',
    include_directories: include_directories,
    sources: sources,
    dependencies: dependencies,
  )
else
  executable(
    'output',
    include_directories: include_directories,
    sources: sources,
    dependencies: dependencies,
  )
endif
