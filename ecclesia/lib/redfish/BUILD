load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

# Description:
# C++ wrapper interfaces for libredfish

licenses(["notice"])

cc_library(
    name = "interface",
    srcs = [
        "interface.cc",
    ],
    hdrs = [
        "interface.h",
        "property.h",
        "property_definitions.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_test(
    name = "interface_test",
    srcs = ["interface_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":interface",
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/public:shared_files",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
    ],
)

cc_library(
    name = "json_ptr",
    srcs = ["json_ptr.cc"],
    hdrs = [
        "json_ptr.h",
    ],
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
    name = "manager",
    srcs = [
        "manager.cc",
    ],
    hdrs = [
        "manager.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
        ":status",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "manager_test",
    srcs = ["manager_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/features/managers:mockup.shar",
    ],
    deps = [
        ":interface",
        ":manager",
        ":property",
        ":property_definitions",
        ":test_mockup",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
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
    name = "authorizer_enums",
    srcs = ["authorizer_enums.cc"],
    hdrs = ["authorizer_enums.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "authorizer_enums_test",
    srcs = ["authorizer_enums_test.cc"],
    deps = [
        ":authorizer_enums",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "property_values",
    hdrs = ["property_values.h"],
    visibility = ["//visibility:public"],
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
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":node_topology",
        ":test_mockup",
        ":topology",
        ":types",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
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
        "//ecclesia/redfish_mockups/features/component_integrity:mockup.shar",
        "//ecclesia/redfish_mockups/topology_v2_multi_host_testing:mockup.shar",
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":node_topology",
        ":test_mockup",
        ":topology_v2",
        ":types",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/testing:node_topology_testing",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "devpath_test",
    srcs = ["devpath_test.cc"],
    deps = [
        ":devpath",
        ":interface",
        ":node_topology",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    srcs = ["types.cc"],
    hdrs = ["types.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "types_test",
    srcs = ["types_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":interface",
        ":test_mockup",
        ":types",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sysmodel",
    srcs = ["sysmodel.cc"],
    hdrs = ["sysmodel.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:function_ref",
    ],
)

cc_test(
    name = "sysmodel_test",
    srcs = ["sysmodel_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":interface",
        ":property_definitions",
        ":sysmodel",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
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
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/network:ip",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:http",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "//ecclesia/lib/redfish/transport:interface",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "test_mockup_test",
    srcs = ["test_mockup_test.cc"],
    data = [
        "@redfishMockupServer//:redfishMockupServer.par",
    ],
    deps = [
        ":interface",
        ":test_mockup",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":property_definitions",
        ":types",
        "//ecclesia/lib/redfish/transport:interface",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":interface",
        ":property_definitions",
        ":test_mockup",
        ":utils",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
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
        ":property_definitions",
        ":topology_config_cc_proto",
        ":topology_configs",
        ":types",
        ":utils",
        "//ecclesia/lib/file:cc_embed_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "node_topology",
    hdrs = ["node_topology.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":topology_config_cc_proto",
        ":types",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

proto_library(
    name = "health_rollup_proto",
    srcs = ["health_rollup.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "health_rollup_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":health_rollup_proto"],
)

cc_library(
    name = "devpath",
    srcs = ["devpath.cc"],
    hdrs = ["devpath.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":node_topology",
        ":types",
        ":utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_json//:json",
    ],
)

cc_library(
    name = "health_rollup",
    srcs = ["health_rollup.cc"],
    hdrs = ["health_rollup.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":health_rollup_cc_proto",
        ":interface",
        ":property_definitions",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "health_rollup_test",
    srcs = ["health_rollup_test.cc"],
    deps = [
        ":health_rollup",
        ":interface",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
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
    cc_namespace = "ecclesia",
    data = glob(["topology_configs/*.textpb"]),
    var_name = "kTopologyConfig",
)

cc_library(
    name = "to_proto",
    srcs = ["to_proto.cc"],
    hdrs = ["to_proto.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "@com_google_absl//absl/status",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "query",
    srcs = ["query.cc"],
    hdrs = ["query.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":sysmodel",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_library(
    name = "status",
    hdrs = ["status.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "host_filter",
    srcs = ["host_filter.cc"],
    hdrs = ["host_filter.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        ":sysmodel",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "host_filter_test",
    srcs = ["host_filter_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/topology_v2_multi_host_testing:mockup.shar",
        "//ecclesia/redfish_mockups/topology_v2_testing:mockup.shar",
    ],
    deps = [
        ":host_filter",
        ":interface",
        ":sysmodel",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "to_proto_test_proto",
    srcs = ["to_proto_test.proto"],
)

cc_proto_library(
    name = "to_proto_test_cc_proto",
    deps = [":to_proto_test_proto"],
)

cc_test(
    name = "to_proto_test",
    srcs = ["to_proto_test.cc"],
    deps = [
        ":to_proto",
        ":to_proto_test_cc_proto",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "query_test",
    srcs = ["query_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_cn_playground:mockup.shar",
        "//ecclesia/redfish_mockups/indus_hmb_shim:mockup.shar",
    ],
    deps = [
        ":interface",
        ":property_definitions",
        ":query",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "node_topology_render_main",
    srcs = ["node_topology_render_main.cc"],
    deps = [
        ":interface",
        ":node_topology",
        ":topology",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/redfish/transport:cache",
        "//ecclesia/lib/redfish/transport:http",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)
