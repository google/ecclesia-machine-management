proto_library(
    name = "query_engine_features_proto",
    srcs = ["query_engine_features.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "query_engine_features_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_engine_features_proto"],
)

cc_library(
    name = "query_engine_features",
    hdrs = ["query_engine_features.h"],
    visibility = ["//visibility:public"],
    deps = [":query_engine_features_cc_proto"],
)
