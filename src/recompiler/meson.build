subdir('generator')

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

run_command(
  'ln',
  '-sf', meson.project_source_root(),
  meson.current_build_dir() / 'charm-src',
  check: true,
)
