# Description:
# C++ wrapper interfaces for libredfish

load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

licenses(["notice"])

cc_library(
    name = "interface",
    hdrs = [
        "interface.h",
        "property.h",
        "property_definitions.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/http:codes",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "json_ptr",
    srcs = ["json_ptr.cc"],
    hdrs = [
        "json_ptr.h",
    ],
    copts = ["-fexceptions"],  # Needed for nlohmann::json's json_ptr.
    features = ["-use_header_modules"],  # Incompatible with -fexceptions.
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_json//:json",
    ],
)

cc_test(
    name = "json_ptr_test",
    srcs = ["json_ptr_test.cc"],
    deps = [
        ":json_ptr",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_library(
    name = "property",
    hdrs = ["property.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "property_definitions",
    hdrs = ["property_definitions.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":property",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "property_values",
    hdrs = ["property_values.h"],
    visibility = ["//visibility:public"],
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
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/thread",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "topology",
    srcs = ["topology.cc"],
    hdrs = ["topology.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":node_topology",
        ":property_definitions",
        ":property_values",
        ":topology_config_cc_proto",
        ":topology_v2",
        ":types",
        ":utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
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
        ":node_topology",
        ":test_mockup",
        ":topology",
        ":types",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "//ecclesia/lib/redfish/testing:node_topology_testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "topology_v2_test",
    srcs = ["topology_v2_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":node_topology",
        ":test_mockup",
        ":topology_v2",
        ":types",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/testing:node_topology_testing",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    hdrs = ["types.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "sysmodel",
    srcs = ["sysmodel.cc"],
    hdrs = ["sysmodel.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
    ],
)

cc_library(
    name = "result",
    hdrs = ["result.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "test_mockup",
    testonly = True,
    srcs = ["test_mockup.cc"],
    hdrs = ["test_mockup.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":raw",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish/transport:http",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "//ecclesia/lib/redfish/transport:interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "utils",
    hdrs = ["utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
    ],
)

cc_library(
    name = "numbers",
    hdrs = ["numbers.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "storage",
    srcs = ["storage.cc"],
    hdrs = ["storage.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
    ],
)

cc_library(
    name = "pci",
    srcs = ["pci.cc"],
    hdrs = ["pci.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":numbers",
        ":property_definitions",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/io/pci:signature",
    ],
)

cc_library(
    name = "topology_v2",
    srcs = ["topology_v2.cc"],
    hdrs = ["topology_v2.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":node_topology",
        ":property",
        ":property_definitions",
        ":topology_config_cc_proto",
        ":topology_configs",
        ":types",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "node_topology",
    hdrs = ["node_topology.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":types",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

proto_library(
    name = "topology_config",
    srcs = ["topology_config.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "topology_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":topology_config"],
)

cc_data_library(
    name = "topology_configs",
    cc_namespace = "libredfish",
    data = glob(["topology_configs/*.textpb"]),
    var_name = "kTopologyConfig",
)
