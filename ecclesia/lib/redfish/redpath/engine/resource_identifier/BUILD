# load("@rules_cc//cc:cc_library.bzl", "cc_library")
# load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "extractor",
    srcs = ["extractor.cc"],
    hdrs = ["extractor.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:property_definitions",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_result",
        "//ecclesia/lib/redfish/redpath/engine:normalizer",
        "//ecclesia/lib/redfish/timing:query_timeout_manager",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_json//:json",
    ],
)

cc_test(
    name = "extractor_test",
    srcs = ["extractor_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        ":extractor",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/redfish/redpath/engine:normalizer",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
