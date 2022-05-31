# Redfish override (a RedfishTransport layer) library

licenses(["notice"])

cc_library(
    name = "transport_with_override",
    srcs = ["transport_with_override.cc"],
    hdrs = ["transport_with_override.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rf_override_cc_proto",
        "//ecclesia/lib/redfish/transport:interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
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
