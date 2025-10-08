# load("@rules_cc//cc:cc_library.bzl", "cc_library")
# load("@rules_cc//cc:cc_test.bzl", "cc_test")

# Description:
# Transport interfaces for Redfish

licenses(["notice"])

cc_library(
    name = "interface",
    hdrs = ["interface.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_library(
    name = "mocked_interface",
    testonly = True,
    hdrs = ["mocked_interface.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "fake_interface",
    testonly = True,
    srcs = ["fake_interface.cc"],
    hdrs = ["fake_interface.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "fake_interface_test",
    srcs = ["fake_interface_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        ":fake_interface",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:variant",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_library(
    name = "grpc_dynamic_fake_server",
    testonly = True,
    srcs = ["grpc_dynamic_fake_server.cc"],
    hdrs = ["grpc_dynamic_fake_server.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_include",
        "//ecclesia/lib/time:clock",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "interface_test",
    srcs = ["interface_test.cc"],
    deps = [
        ":interface",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "grpc_dynamic_fake_server_test",
    srcs = ["grpc_dynamic_fake_server_test.cc"],
    deps = [
        ":cache",
        ":grpc",
        ":grpc_dynamic_fake_server",
        ":http_redfish_intf",
        ":interface",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:utils",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_json//:json",
    ],
)

cc_library(
    name = "grpc_tls_options",
    srcs = ["grpc_tls_options.cc"],
    hdrs = ["grpc_tls_options.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "cache",
    srcs = ["cache.cc"],
    hdrs = ["cache.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "//ecclesia/lib/complexity_tracker",
        "//ecclesia/lib/redfish/timing:query_timeout_manager",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_json//:json",
    ],
)

cc_library(
    name = "logged_transport",
    srcs = ["logged_transport.cc"],
    hdrs = ["logged_transport.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "//ecclesia/lib/redfish:utils",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_json//:json",
    ],
)

cc_library(
    name = "http",
    srcs = ["http.cc"],
    hdrs = ["http.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:property_definitions",
        "//ecclesia/lib/redfish:utils",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_json//:json",
    ],
)

cc_test(
    name = "http_test",
    srcs = ["http_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":http",
        ":interface",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/public:shared_files",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
        "@com_json//:json",
    ],
)

cc_library(
    name = "grpc",
    srcs = ["grpc.cc"],
    hdrs = ["grpc.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":struct_proto_conversion",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:utils",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_include",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/time:clock",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc_security_base",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
        "@com_json//:json",
    ],
)

cc_test(
    name = "grpc_test",
    srcs = ["grpc_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":grpc",
        ":grpc_tls_options",
        ":interface",
        ":struct_proto_conversion",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish:utils",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/testing:grpc_dynamic_mockup_server",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:clock_fake",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_json//:json",
    ],
)

cc_library(
    name = "http_redfish_intf",
    srcs = ["http_redfish_intf.cc"],
    hdrs = ["http_redfish_intf.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cache",
        ":interface",
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:json_ptr",
        "//ecclesia/lib/redfish:utils",
        "//ecclesia/lib/redfish/timing:query_timeout_manager",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_test(
    name = "http_redfish_intf_test",
    srcs = ["http_redfish_intf_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":cache",
        ":grpc",
        ":grpc_tls_options",
        ":http",
        ":http_redfish_intf",
        ":interface",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/testing:grpc_dynamic_mockup_server",
        "//ecclesia/lib/redfish/timing:query_timeout_manager",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/thread",
        "//ecclesia/lib/time:clock_fake",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
        "@com_json//:json",
    ],
)

cc_library(
    name = "struct_proto_conversion",
    srcs = ["struct_proto_conversion.cc"],
    hdrs = ["struct_proto_conversion.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:protobuf",
        "@com_json//:json",
    ],
)

cc_test(
    name = "struct_proto_conversion_test",
    srcs = ["struct_proto_conversion_test.cc"],
    visibility = ["//visibility:private"],
    deps = [
        ":struct_proto_conversion",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_json//:json",
    ],
)

cc_library(
    name = "x509_certificate",
    srcs = ["x509_certificate.cc"],
    hdrs = ["x509_certificate.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/status:macros",
        "@boringssl//:crypto",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "grpc_certificate_verifier",
    srcs = ["grpc_certificate_verifier.cc"],
    hdrs = ["grpc_certificate_verifier.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":x509_certificate",
        "//ecclesia/lib/status:rpc",
        "@boringssl//:crypto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

proto_library(
    name = "transport_metrics_proto",
    srcs = ["transport_metrics.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "transport_metrics_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":transport_metrics_proto"],
)

cc_library(
    name = "metrical_transport",
    srcs = ["metrical_transport.cc"],
    hdrs = ["metrical_transport.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":transport_metrics_cc_proto",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "metrical_transport_test",
    srcs = ["metrical_transport_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":grpc",
        ":grpc_tls_options",
        ":metrical_transport",
        "//ecclesia/lib/redfish/testing:grpc_dynamic_mockup_server",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)
