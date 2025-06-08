project(
    'dimalloc',
    'c',
    version: '0.1.0',
    default_options: ['warning_level=3', 'c_std=c11']
)

includes = include_directories('include')
src = [
    'src/dimalloc_mman.c',      # Allocate & free
    'src/dimalloc_global.c',    # Global pool
    'src/dimalloc_init_unix.c', # Create & destroy pools on Unix systems
    'src/dimalloc_init_win32.c' # Create & destroy pools on Windows systems
]
test_src = [
    'test/main.c'
]

dimalloc = shared_library(
    'dimalloc',
    src,
    include_directories: includes,
    install: true,
    c_args: ['-DBUILDING_MESON_LIBRARY']
)

test_executable = executable(
    'dimalloc_test',
    test_src,
    include_directories: includes,
    link_with: [dimalloc]
)

test('test', test_executable)

dimalloc_dep = declare_dependency(
    include_directories: includes,
    link_with: [dimalloc]
)
