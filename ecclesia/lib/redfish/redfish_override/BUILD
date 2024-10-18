# Redfish override (a RedfishTransport layer) library

load("//ecclesia/build_defs:proto.bzl", "proto_data")

licenses(["notice"])

proto_data(
    name = "test_policy",
    src = "redfish_override_policy/test_policy.textproto",
    out = "test_policy.binarypb",
    proto_deps = [
        ":rf_override_proto",
    ],
    proto_name = "ecclesia.OverridePolicy",
    visibility = [
        "//visibility:public",
    ],
)

cc_library(
    name = "transport_with_override",
    srcs = ["transport_with_override.cc"],
    hdrs = ["transport_with_override.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rf_override_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_include",
        "//ecclesia/lib/redfish/transport:grpc",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/redfish/transport:struct_proto_conversion",
        "@com_github_grpc_grpc//:grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "transport_with_override_test",
    srcs = ["transport_with_override_test.cc"],
    data = [
        ":test_policy",
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":rf_override_cc_proto",
        ":transport_with_override",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/testing:grpc_dynamic_mockup_server",
        "//ecclesia/lib/redfish/transport:grpc_tls_options",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/redfish/transport:mocked_interface",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

proto_library(
    name = "rf_override_proto",
    srcs = ["rf_override.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:struct_proto",
    ],
)

cc_proto_library(
    name = "rf_override_cc_proto",
    visibility = [
        "//visibility:public",
    ],
    deps = [":rf_override_proto"],
)
