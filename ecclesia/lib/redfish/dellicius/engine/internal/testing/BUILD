licenses(["notice"])

cc_test(
    name = "query_planner_test",
    srcs = ["query_planner_test.cc"],
    data = [
        "query_in/assembly_in.textproto",
        "query_in/sensor_in.textproto",
        "query_out/assembly_out.textproto",
        "query_out/devpath_sensor_out.textproto",
        "query_out/sensor_out.textproto",
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:normalizer",
        "//ecclesia/lib/redfish/dellicius/engine/internal:query_planner",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/time:clock_fake",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
