# Transport interfaces for Redfish

licenses(["notice"])

cc_library(
    name = "interface",
    hdrs = ["interface.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_json//:json",
    ],
)

cc_library(
    name = "proto_variant",
    srcs = ["proto_variant.cc"],
    hdrs = ["proto_variant.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish:interface",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_variant_test",
    srcs = ["proto_variant_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":proto_variant",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
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
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "grpc_dynamic_fake_server_test",
    srcs = ["grpc_dynamic_fake_server_test.cc"],
    deps = [
        ":grpc_dynamic_fake_server",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_proto",
        "//ecclesia/lib/testing:proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "grpc_dynamic_options",
    srcs = ["grpc_dynamic_options.cc"],
    hdrs = ["grpc_dynamic_options.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/logging",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "grpc_dynamic",
    srcs = ["grpc_dynamic_impl.cc"],
    hdrs = ["grpc_dynamic_impl.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":grpc_dynamic_options",
        ":proto_variant",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_proto",
        "//ecclesia/lib/status:rpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "grpc_dynamic_test",
    srcs = ["grpc_dynamic_impl_test.cc"],
    deps = [
        ":grpc_dynamic",
        ":grpc_dynamic_fake_server",
        ":grpc_dynamic_options",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/testing:status",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "cache",
    srcs = ["cache.cc"],
    hdrs = ["cache.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
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
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
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
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
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
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_proto",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/time:clock",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_public_hdrs",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
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
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/testing:grpc_dynamic_mockup_server",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/testing:status",
        "@com_google_googletest//:gtest_main",
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
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:json_ptr",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
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
        ":http",
        ":http_redfish_intf",
        ":interface",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
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
    visibility = ["//visibility:public"],
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
