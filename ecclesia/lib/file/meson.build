file_include = all_include

deps_for_path = [
    dependency('absl_base'),
    dependency('absl_span'),
    dependency('absl_strings'),
    dependency('absl_log'),
    dependency('absl_string_view'),
]

lib_path = static_library(
  'path',
  'path.cc',
  include_directories: file_include,
  implicit_include_directories: false,
  dependencies: [deps_for_path],
)

dep_path = declare_dependency(
  dependencies: deps_for_path,
  include_directories: file_include,
  link_with: [lib_path],
)

deps_for_uds = [
    dependency('absl_base'),
    dependency('absl_log'),
    dependency('absl_string_view'),
    dep_path,
]

lib_install_dir = get_option('libdir')
lib_uds = library(
  'uds',
  'uds.cc',
  include_directories: file_include,
  implicit_include_directories: false,
  dependencies: [deps_for_uds],
  install: true,
  install_dir: lib_install_dir,
  version: meson.project_version(),
)
install_headers('uds.h', subdir: 'ecclesia/lib/file' )

pkg.generate(lib_uds,
  name: 'ecclesia-uds',
)

dep_uds= declare_dependency(
  dependencies: deps_for_uds,
  include_directories: file_include,
  link_with: [lib_uds],
)
