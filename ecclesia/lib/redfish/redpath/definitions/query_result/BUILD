proto_library(
    name = "query_result_proto",
    srcs = ["query_result.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish/transport:transport_metrics_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "query_result_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_result_proto"],
)

cc_library(
    name = "query_result",
    srcs = ["query_result.cc"],
    hdrs = ["query_result.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_result_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "query_result_test",
    srcs = ["query_result_test.cc"],
    deps = [
        ":query_result",
        ":query_result_cc_proto",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "converter",
    srcs = ["converter.cc"],
    hdrs = ["converter.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_result",
        ":query_result_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/transport:transport_metrics_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_test(
    name = "converter_test",
    srcs = ["converter_test.cc"],
    deps = [
        ":converter",
        ":query_result_cc_proto",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@com_json//:json",
    ],
)