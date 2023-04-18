load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

licenses(["notice"])

proto_library(
    name = "query_proto",
    srcs = [
        "query.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

proto_library(
    name = "query_result_proto",
    srcs = [
        "query_result.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "query_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_proto"],
)

cc_proto_library(
    name = "query_result_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_result_proto"],
)

py_proto_library(
    name = "query_py_pb2",
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
    deps = [":query_proto"],
)

py_proto_library(
    name = "query_result_py_pb2",
    visibility = ["//visibility:public"],
    deps = [":query_result_proto"],
)
