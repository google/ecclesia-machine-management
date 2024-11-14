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
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:redpath_subscription",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
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
        "//ecclesia/lib/redfish:property",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "path_util",
    srcs = ["path_util.cc"],
    hdrs = ["path_util.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":converter",
        ":query_result_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "path_util_test",
    srcs = ["path_util_test.cc"],
    deps = [
        ":path_util",
        ":query_result_cc_proto",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "query_result_verification_proto",
    srcs = ["query_result_verification.proto"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_result_proto",
    ],
)

cc_proto_library(
    name = "query_result_verification_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_result_verification_proto"],
)

cc_library(
    name = "verification",
    srcs = ["verification.cc"],
    hdrs = ["verification.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":converter",
        ":query_result",
        ":query_result_cc_proto",
        ":query_result_verification_cc_proto",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "verification_test",
    srcs = ["verification_test.cc"],
    deps = [
        ":query_result_cc_proto",
        ":query_result_verification_cc_proto",
        ":verification",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
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
        "@com_google_googleapis//google/rpc:code_cc_proto",
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

cc_library(
    name = "query_result_validator",
    srcs = ["query_result_validator.cc"],
    hdrs = ["query_result_validator.h"],
    deps = [
        ":query_result_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "query_result_validator_test",
    srcs = ["query_result_validator_test.cc"],
    deps = [
        ":query_result_validator",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
