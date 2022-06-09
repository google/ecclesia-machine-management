licenses(["notice"])

cc_library(
    name = "grpc",
    srcs = ["grpc.cc"],
    hdrs = ["grpc.h"],
    visibility = ["//ecclesia:mmanager_frontend_users"],
    deps = [
        "//ecclesia/lib/atomic:sequence",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/status:rpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "grpc_test",
    srcs = ["grpc_test.cc"],
    deps = [
        ":grpc",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_github_grpc_grpc//:grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
