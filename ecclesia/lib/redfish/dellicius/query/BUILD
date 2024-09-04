load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

licenses(["notice"])

proto_library(
    name = "query_proto",
    srcs = [
        "query.proto",
    ],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "query_result_proto",
    srcs = [
        "query_result.proto",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":query_errors_proto",
        "//ecclesia/lib/redfish/transport:transport_metrics_proto",
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

proto_library(
    name = "query_variables_proto",
    srcs = [
        "query_variables.proto",
    ],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "query_errors_proto",
    srcs = [
        "query_errors.proto",
    ],
    visibility = ["//visibility:public"],
    deps = ["@com_google_googleapis//google/rpc:status_proto"],
)

cc_proto_library(
    name = "query_errors_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_errors_proto"],
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

cc_proto_library(
    name = "query_variables_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_variables_proto"],
)

py_proto_library(
    name = "query_py_pb2",
    visibility = ["//visibility:private"],
    deps = [":query_proto"],
)

py_proto_library(
    name = "query_result_py_pb2",
    visibility = ["//visibility:public"],
    deps = [":query_result_proto"],
)

py_proto_library(
    name = "query_variables_py_pb2",
    visibility = ["//visibility:public"],
    deps = [":query_variables_proto"],
)

cc_library(
    name = "builder",
    srcs = ["builder.cc"],
    hdrs = ["builder.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/file:path",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_binary(
    name = "query_spec_generator",
    srcs = ["query_spec_generator.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":builder",
        ":query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:query_validator",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)
