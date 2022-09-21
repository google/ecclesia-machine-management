licenses(["notice"])

proto_library(
    name = "query_proto",
    srcs = ["query.proto"],
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
