include_directories = include_directories(
  '../external/ELFIO',
  '../include',
)

if not get_option('runtime_only')
  subdir('isa')
  subdir('recompiler')
  subdir('debugger')
endif

subdir('runtime')
