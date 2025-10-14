project('output', 'cpp', default_options: ['b_lto=true'])

add_project_arguments('-Wno-unused-label', language: 'cpp')

if get_option('debugging')
  add_project_arguments('-DLAYER_DEBUG', language: 'cpp')
endif

include_directories: [
  'include',
]

sources = files(
  'src/armv4.cpp',
  'src/code.cpp',
  'src/data.cpp',
  'src/debug.cpp',
  'src/memory.cpp',
)

dependencies: [
]

if get_option('library')
  output_dep = library(
    'output',
    include_directories: include_directories,
    sources: sources,
    dependencies: dependencies,
  )
else
  output_dep = executable(
    'output',
    include_directories: include_directories,
    sources: sources,
    dependencies: dependencies,
  )
endif
