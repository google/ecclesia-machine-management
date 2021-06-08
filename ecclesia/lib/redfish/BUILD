# Description:
# C++ wrapper interfaces for libredfish
licenses(["notice"])

cc_library(
    name = "interface",
    hdrs = ["interface.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "property",
    hdrs = ["property.h"],
    visibility = ["//ecclesia:redfish_users"],
)

cc_library(
    name = "property_definitions",
    hdrs = ["property_definitions.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [":property"],
)

cc_library(
    name = "property_values",
    hdrs = ["property_values.h"],
    visibility = ["//ecclesia:redfish_users"],
)

cc_library(
    name = "raw",
    srcs = [
        "libredfish_adapter.cc",
        "raw.cc",
    ],
    hdrs = [
        "libredfish_adapter.h",
        "raw.h",
    ],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/thread",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
        "@libredfish",
    ],
)

cc_test(
    name = "raw_test",
    srcs = ["raw_test.cc"],
    data = [
        "//ecclesia/lib/redfish/testing:testing_cert",
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
    ],
    deps = [
        ":interface",
        ":raw",
        ":test_mockup",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "topology",
    srcs = ["topology.cc"],
    hdrs = ["topology.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        ":property_definitions",
        ":types",
        ":utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "topology_test",
    srcs = ["topology_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_cn_playground:mockup.shar",
    ],
    deps = [
        ":test_mockup",
        ":topology",
        ":types",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    hdrs = ["types.h"],
    visibility = [
        "//ecclesia:mmanager_backend_users",
        "//ecclesia:redfish_users",
    ],
    deps = [],
)

cc_library(
    name = "sysmodel",
    srcs = ["sysmodel.cc"],
    hdrs = ["sysmodel.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        ":property_definitions",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "result",
    hdrs = ["result.h"],
    visibility = [
        "//ecclesia:mmanager_backend_users",
        "//ecclesia:redfish_users",
    ],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "test_mockup",
    testonly = True,
    srcs = ["test_mockup.cc"],
    hdrs = ["test_mockup.h"],
    visibility = [
        "//ecclesia:mmanager_backend_users",
        "//ecclesia:redfish_users",
    ],
    deps = [
        ":interface",
        ":raw",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "//ecclesia/lib/network:testing",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "utils",
    hdrs = ["utils.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        ":property_definitions",
    ],
)

cc_library(
    name = "numbers",
    hdrs = ["numbers.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "storage",
    srcs = ["storage.cc"],
    hdrs = ["storage.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        ":property_definitions",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "pci",
    srcs = ["pci.cc"],
    hdrs = ["pci.h"],
    visibility = ["//ecclesia:redfish_users"],
    deps = [
        ":interface",
        ":numbers",
        ":property_definitions",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/io/pci:signature",
        "@com_google_absl//absl/types:optional",
    ],
)
