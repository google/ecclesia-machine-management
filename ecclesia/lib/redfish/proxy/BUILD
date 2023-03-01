licenses(["notice"])

cc_library(
    name = "redfish_proxy",
    hdrs = ["redfish_proxy.h"],
    visibility = ["//ecclesia:mmanager_frontend_users"],
    deps = [
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_include",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "redfish_proxy_cc",
    srcs = ["redfish_proxy.cc"],
    hdrs = ["redfish_proxy.h"],
    visibility = ["//ecclesia:mmanager_frontend_users"],
    deps = [
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_include",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "grpc",
    srcs = ["grpc_impl.cc"],
    hdrs = ["grpc_impl.h"],
    visibility = ["//ecclesia:mmanager_frontend_users"],
    deps = [
        ":redfish_proxy",
        "//ecclesia/lib/atomic:sequence",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/status:rpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "grpc_impl_test",
    srcs = ["grpc_impl_test.cc"],
    deps = [
        ":grpc",
        ":redfish_proxy",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proxy:redfish_proxy_cc",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc_security_base",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "redfish_impl",
    srcs = ["redfish_impl.cc"],
    hdrs = ["redfish_impl.h"],
    visibility = ["//ecclesia:mmanager_frontend_users"],
    deps = [
        ":redfish_proxy",
        "//ecclesia/lib/atomic:sequence",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:utils",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proxy:redfish_proxy_cc",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/status:rpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "redfish_impl_test",
    srcs = ["redfish_impl_test.cc"],
    deps = [
        ":redfish_impl",
        ":redfish_proxy",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proxy:redfish_proxy_cc",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc_security_base",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
