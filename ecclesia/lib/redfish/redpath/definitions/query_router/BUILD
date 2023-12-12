proto_library(
    name = "query_router_spec_proto",
    srcs = ["query_router_spec.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_proto",
    ],
)

cc_proto_library(
    name = "query_router_spec_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_router_spec_proto"],
)
