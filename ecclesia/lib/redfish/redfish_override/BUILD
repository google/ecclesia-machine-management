# Redfish override (a RedfishTransport layer) library

licenses(["notice"])

cc_library(
    name = "transport_with_override",
    srcs = ["transport_with_override.cc"],
    hdrs = ["transport_with_override.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rf_override_cc_proto",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/redfish/transport:struct_proto_conversion",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_test(
    name = "transport_with_override_test",
    srcs = ["transport_with_override_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/redfish_override:transport_with_override",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/redfish/transport:mocked_interface",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
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
    visibility = ["//visibility:public"],
    deps = [":rf_override_proto"],
)
