load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

cc_library(
    name = "query_engine_config",
    hdrs = ["config.h"],
    visibility = ["//visibility:public"],
    deps = ["//ecclesia/lib/file:cc_embed_interface"],
)

cc_library(
    name = "query_engine_cc",
    srcs = ["query_engine.cc"],
    hdrs = ["query_engine.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_engine_config",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:node_topology",
        "//ecclesia/lib/redfish:topology",
        "//ecclesia/lib/redfish/dellicius/engine/internal:factory",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:parsers",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

proto_library(
    name = "query_rules_proto",
    srcs = ["query_rules.proto"],
)

cc_proto_library(
    name = "query_rules_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_rules_proto"],
)

filegroup(
    name = "sample_query_rules_in",
    srcs = [
        "sample_query_rules.textproto",
    ],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "redpath_query_bzl",
    srcs = ["redpath_query.bzl"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/build_defs:embed",
        "//ecclesia/build_defs:proto",
    ],
)
