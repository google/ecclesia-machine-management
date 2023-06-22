load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

licenses(["notice"])

cc_test(
    name = "query_planner_test",
    srcs = ["query_planner_test.cc"],
    data = [
        "//ecclesia/lib/redfish/dellicius/query/samples:sample_queries_in",
        "//ecclesia/lib/redfish/dellicius/query/samples:sample_queries_out",
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:topology",
        "//ecclesia/lib/redfish/dellicius/engine:factory",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:query_planner",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "//ecclesia/lib/redfish/transport:metrical_transport",
        "//ecclesia/lib/redfish/transport:transport_metrics_cc_proto",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
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
    name = "test_query_rules_embedded",
    cc_namespace = "ecclesia",
    data = [
        "//ecclesia/lib/redfish/dellicius/engine:sample_query_rules_in",
    ],
    var_name = "kQueryRules",
)

cc_test(
    name = "query_engine_test",
    srcs = ["query_engine_test.cc"],
    data = [
        "//ecclesia/lib/redfish/dellicius/query/samples:sample_queries_out",
        "//ecclesia/redfish_mockups/features/component_integrity:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        ":test_queries_embedded",
        ":test_query_rules_embedded",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_cc",
        "//ecclesia/lib/redfish/dellicius/engine:query_engine_fake",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/transport:transport_metrics_cc_proto",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
