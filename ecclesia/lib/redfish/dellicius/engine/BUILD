licenses(["notice"])

cc_library(
    name = "query_engine_config",
    hdrs = ["config.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/file:cc_embed_interface",
    ],
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
        "//ecclesia/lib/redfish/dellicius/engine/internal:factory",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)
