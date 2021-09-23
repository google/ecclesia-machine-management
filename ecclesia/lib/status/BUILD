licenses(["notice"])

cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    visibility = ["//ecclesia:library_users"],
)

cc_library(
    name = "posix",
    srcs = ["posix.cc"],
    hdrs = ["posix.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "rpc",
    srcs = ["rpc.cc"],
    hdrs = ["rpc.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_grpc_grpc//:grpc++_public_hdrs",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "test_macros",
    testonly = 1,
    hdrs = ["test_macros.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":macros",
    ],
)
