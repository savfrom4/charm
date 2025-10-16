project('output', 'cpp', default_options: ['b_lto=true'])
charm_proj = subproject('charm', default_options: ['runtime_only=true'])

add_project_arguments('-Wno-unused-label', language: 'cpp')

if get_option('debugging')
  add_project_arguments('-DLAYER_DEBUG', language: 'cpp')
endif

include_directories = [
  'include',
]

sources = files(
  'src/code.cpp',
  'src/data.cpp',
)

dependencies = [
  charm_proj.get_variable('libruntime_dep'),
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
