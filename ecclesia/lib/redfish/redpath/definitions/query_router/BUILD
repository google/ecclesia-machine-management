load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

proto_library(
    name = "query_router_spec_proto",
    srcs = ["query_router_spec.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_engine_features_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_verification_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "query_router_spec_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_router_spec_proto"],
)

py_proto_library(
    name = "query_router_spec_py_pb2",
    visibility = ["//visibility:public"],
    deps = [":query_router_spec_proto"],
)

cc_library(
    name = "query_router",
    srcs = ["query_router.cc"],
    hdrs = ["query_router.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":default_template_variable_names",
        ":query_router_spec_cc_proto",
        ":util",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "//ecclesia/lib/redfish/dellicius/query:query_variables_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:id_assigner",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_engine_features",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_engine_features_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_spec",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/thread:thread_pool",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "query_router_test",
    srcs = ["query_router_test.cc"],
    deps = [
        ":default_template_variable_names",
        ":query_router",
        ":query_router_spec_cc_proto",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine:file_backed_query_engine",
        "//ecclesia/lib/redfish/dellicius/engine:mock_query_engine",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "//ecclesia/lib/redfish/dellicius/query:query_variables_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:id_assigner",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_spec",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "query_router_mock",
    testonly = True,
    hdrs = ["query_router_mock.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_router",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_router_spec_cc_proto",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_spec",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "default_template_variable_names",
    hdrs = ["default_template_variable_names.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
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
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_engine:query_spec",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

exports_files(
    ["default_template_variables.textproto"],
    visibility = ["//visibility:public"],
)
