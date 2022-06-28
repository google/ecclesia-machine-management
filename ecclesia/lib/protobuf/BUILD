load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

licenses(["notice"])

cc_library(
    name = "field_mask",
    srcs = ["field_mask.cc"],
    hdrs = ["field_mask.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "parse",
    hdrs = ["parse.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

py_library(
    name = "compare",
    testonly = True,
    srcs = ["compare.py"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_protobuf//:protobuf_python",
    ],
)

py_test(
    name = "compare_test",
    srcs = ["compare_test.py"],
    deps = [
        ":compare",
        ":compare_test_proto_py_pb2",
        "@com_google_absl_py//absl/testing:absltest",
    ],
)

proto_library(
    name = "compare_test_proto",
    srcs = ["compare_test.proto"],
)

py_proto_library(
    name = "compare_test_proto_py_pb2",
    deps = [":compare_test_proto"],
)
