load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

licenses(["notice"])

cc_library(
    name = "query_engine_cc",
    srcs = ["query_engine.cc"],
    hdrs = ["query_engine.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":factory",
        ":query_rules_cc_proto",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:topology",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "//ecclesia/lib/redfish/dellicius/engine/internal:query_planner",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_errors_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_variables_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:id_assigner",
        "//ecclesia/lib/redfish/dellicius/utils:parsers",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:converter",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/redfish/transport:metrical_transport",
        "//ecclesia/lib/redfish/transport:transport_metrics_cc_proto",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/time:clock",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "file_backed_query_engine",
    srcs = ["file_backed_query_engine.cc"],
    hdrs = ["file_backed_query_engine.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_engine_cc",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "file_backed_query_engine_test",
    srcs = ["file_backed_query_engine_test.cc"],
    deps = [
        ":file_backed_query_engine",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/redpath/definitions/query_result",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mock_query_engine",
    testonly = 1,
    hdrs = ["mock_query_engine.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_engine_cc",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "fake_query_engine",
    testonly = 1,
    hdrs = ["fake_query_engine.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_engine_cc",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

proto_library(
    name = "query_rules_proto",
    srcs = ["query_rules.proto"],
    visibility = ["//visibility:public"],
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
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
    deps = [
        "//ecclesia/build_defs:embed",
        "//ecclesia/build_defs:proto",
    ],
)

cc_library(
    name = "factory",
    hdrs = ["factory.h"],
    visibility = [
        "//ecclesia/lib/redfish:__subpackages__",
        "//platforms/redfish/lib/query_engine:__subpackages__",
    ],
    deps = [
        "//ecclesia/lib/redfish:node_topology",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:normalizer",
        "//ecclesia/lib/redfish/dellicius/utils:id_assigner",
        "@com_google_absl//absl/memory",
    ],
)

cc_data_library(
    name = "test_queries_embedded",
    cc_namespace = "ecclesia",
    data = [
        "//ecclesia/lib/redfish/dellicius/query/samples:sample_queries_in",
    ],
    var_name = "kDelliciusQueries",
)

cc_data_library(
    name = "test_malformed_queries_embedded",
    cc_namespace = "ecclesia",
    data = [
        "//ecclesia/lib/redfish/dellicius/query/samples:sample_malformed_queries_in",
    ],
    var_name = "kMalformedDelliciusQueries",
)

cc_data_library(
    name = "test_query_rules_embedded",
    cc_namespace = "ecclesia",
    data = [
        ":sample_query_rules_in",
    ],
    var_name = "kQueryRules",
)

cc_test(
    name = "query_engine_test",
    srcs = ["query_engine_test.cc"],
    data = [
        "//ecclesia/lib/redfish/dellicius/query/samples:sample_queries_out",
        "//ecclesia/redfish_mockups/features/component_integrity:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        ":fake_query_engine",
        ":query_engine_cc",
        ":query_rules_cc_proto",
        ":test_queries_embedded",
        ":test_query_rules_embedded",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/engine/internal:passkey",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_variables_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:id_assigner",
        "//ecclesia/lib/redfish/dellicius/utils:id_assigner_devpath",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/transport:http",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/redfish/transport:transport_metrics_cc_proto",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:clock",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/public:shared_files",
    ],
)
