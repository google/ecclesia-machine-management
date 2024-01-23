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

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        ":query_router_spec_cc_proto",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "util_test",
    srcs = ["util_test.cc"],
    deps = [
        ":query_router_spec_cc_proto",
        ":util",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
