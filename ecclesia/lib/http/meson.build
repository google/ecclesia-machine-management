http_include = all_include

protoc = find_program('protoc')
grpc_plugin = find_program('grpc_cpp_plugin')
proto_gen = generator(protoc,
  output : [
    '@BASENAME@.pb.cc',
    '@BASENAME@.pb.h',
    ],
  arguments : [
    '--proto_path=@CURRENT_SOURCE_DIR@',
    '--grpc_out=@BUILD_DIR@',
    '--cpp_out=@BUILD_DIR@',
    '--plugin=protoc-gen-grpc='+grpc_plugin.path(),
    '@INPUT@'])
grpc_gen = generator(protoc,
  output : [
    '@BASENAME@.grpc.pb.cc',
    '@BASENAME@.grpc.pb.h',
    ],
  arguments : [
    '--proto_path=@CURRENT_SOURCE_DIR@',
    '--grpc_out=@BUILD_DIR@',
    '--cpp_out=@BUILD_DIR@',
    '--plugin=protoc-gen-grpc='+grpc_plugin.path(),
    '@INPUT@'])
protobuf_generated = [
  proto_gen.process(
    [
      'cred.proto',
    ],
    preserve_path_from : meson.current_source_dir()),
  grpc_gen.process(
    [
      'cred.proto',
    ],
    preserve_path_from : meson.current_source_dir())
]
cpp_compiler = meson.get_compiler('cpp')
grpc_deps = dependency('grpc', required: false)
if not grpc_deps.found()
  grpc_deps = cpp_compiler.find_library('grpc')
endif
grpcpp_deps = dependency('grpc++', required: false)
if not grpcpp_deps.found()
  grpcpp_deps = cpp_compiler.find_library('grpc++')
endif
gpr_deps = dependency('gpr', required: false)
if not gpr_deps.found()
  gpr_deps = cpp_compiler.find_library('gpr')
endif

http_proto_deps = [
  gpr_deps,
  grpc_deps,
  grpcpp_deps,
  dependency('protobuf'),
  cpp_compiler.find_library('grpc++_reflection'),
]
libhttp_proto = library(
  'vbmc_proto',
  protobuf_generated,
  implicit_include_directories: false,
  dependencies: http_proto_deps,
)
http_proto = declare_dependency(
  sources: [protobuf_generated],
  dependencies: http_proto_deps,
  link_with: [libhttp_proto],
)


nlohmann_json = declare_dependency()

deps_for_http = [
    dependency('absl_base'),
    dependency('absl_statusor'),
    dependency('absl_flat_hash_map'),
    dependency('absl_log'),
    dependency('absl_status'),
    dependency('absl_check'),
    dependency('absl_string_view'),
    nlohmann_json,
]

lib_install_dir = get_option('libdir')
lib_http_client = library(
  'client',
  'client.cc',
  include_directories: http_include,
  implicit_include_directories: false,
  dependencies: [deps_for_http],
  install: true,
  install_dir: lib_install_dir,
  version: meson.project_version(),
)
install_headers('client.h', subdir: 'ecclesia/lib/http')

pkg.generate(lib_http_client,
  name: 'ecclesia-http-client',
)

dep_http_client = declare_dependency(
  dependencies: deps_for_http,
  include_directories: http_include,
  link_with: [lib_http_client],
)

lib_codes = library(
  'codes',
  'codes.cc',
  include_directories: http_include,
  implicit_include_directories: false,
  dependencies: [deps_for_codes],
  install: true,
  install_dir: lib_install_dir,
  version: meson.project_version(),
)
install_headers('codes.h', subdir: 'ecclesia/lib/http')
pkg.generate(lib_codes,
  name: 'ecclesia-codes',
  )

dep_codes = declare_dependency(
  dependencies: deps_for_codes,
  include_directories: http_include,
  link_with: [lib_codes],
)


deps_for_curl_client = [
    dependency('absl_base'),
    dependency('absl_core_headers'),
    dependency('absl_cleanup'),
    dependency('absl_statusor'),
    dependency('absl_flat_hash_map'),
    dependency('absl_log'),
    dependency('absl_status'),
    dependency('absl_check'),
    dependency('absl_string_view'),
    dependency('absl_strings'),
    dependency('absl_str_format'),
    dependency('absl_synchronization'),
    dependency('libcurl'),
    http_proto,
]

lib_curl_client = library(
  'curl_client',
  'curl_client.cc',
  include_directories: http_include,
  implicit_include_directories: false,
  dependencies: [deps_for_curl_client],
  install: true,
  install_dir: lib_install_dir,
  version: meson.project_version(),
)
install_headers('curl_client.h', subdir: 'ecclesia/lib/http')
pkg.generate(lib_curl_client,
  name: 'ecclesia-curl-client',
)

dep_curl_client = declare_dependency(
  dependencies: deps_for_curl_client,
  include_directories: http_include,
  link_with: [lib_curl_client],
)
