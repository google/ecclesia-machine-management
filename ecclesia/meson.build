
project(
  'ecclesia-mm',
  ['c', 'cpp'],
  version: '0.1',
  meson_version: '>=1.1.1',
  default_options: [
    'cpp_std=c++23',
    'c_std=c18',
  ],
)
cppc = meson.get_compiler('cpp')

all_include = include_directories('.')
pkg = import('pkgconfig')
subdir('lib')
