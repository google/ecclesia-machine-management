# Description:
# C++ testing utilities for Ecclesia's Redfish interface
licenses(["notice"])

cc_library(
    name = "json_mockup",
    testonly = True,
    srcs = ["json_mockup.cc"],
    hdrs = ["json_mockup.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:interface",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@jansson",
    ],
)

cc_test(
    name = "json_mockup_test",
    srcs = ["json_mockup_test.cc"],
    deps = [
        ":json_mockup",
        "//ecclesia/lib/redfish:interface",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

filegroup(
    name = "testing_cert",
    srcs = glob([
        "cert/*.crt",
        "cert/*.key",
    ]),
    visibility = ["//ecclesia:redfish_users"],
)

cc_library(
    name = "patchable_mockup_server",
    testonly = True,
    srcs = ["patchable_mockup_server.cc"],
    hdrs = ["patchable_mockup_server.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/file:uds",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:raw",
        "//ecclesia/lib/redfish:test_mockup",
        "//ecclesia/magent/daemons:magent_hdr",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
    ],
)

cc_test(
    name = "patchable_mockup_server_test",
    srcs = ["patchable_mockup_server_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
    ],
    deps = [
        ":patchable_mockup_server",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:property",
        "//ecclesia/lib/redfish:property_definitions",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "node_topology_testing",
    testonly = True,
    srcs = ["node_topology_testing.cc"],
    hdrs = ["node_topology_testing.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        "//ecclesia/lib/redfish:node_topology",
        "//ecclesia/lib/redfish:types",
        "@com_google_absl//absl/strings:str_format",
    ],
)
