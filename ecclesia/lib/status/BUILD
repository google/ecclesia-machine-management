# load("@rules_cc//cc:cc_library.bzl", "cc_library")

licenses(["notice"])

cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "posix",
    srcs = ["posix.cc"],
    hdrs = ["posix.h"],
    visibility = ["//visibility:public"],
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
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "test_macros",
    testonly = 1,
    hdrs = ["test_macros.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":macros",
    ],
)
