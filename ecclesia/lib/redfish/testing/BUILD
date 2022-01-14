# Description:
# C++ testing utilities for Ecclesia's Redfish interface
licenses(["notice"])

cc_library(
    name = "json_mockup",
    testonly = True,
    srcs = ["json_mockup.cc"],
    hdrs = ["json_mockup.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:interface",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_test(
    name = "json_mockup_test",
    srcs = ["json_mockup_test.cc"],
    deps = [
        ":json_mockup",
        "//ecclesia/lib/redfish:interface",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

filegroup(
    name = "testing_cert",
    srcs = glob([
        "cert/*.crt",
        "cert/*.key",
    ]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "fake_redfish_server",
    testonly = True,
    srcs = ["fake_redfish_server.cc"],
    hdrs = ["fake_redfish_server.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:test_mockup",
        "//ecclesia/lib/redfish/transport:http",
        "//ecclesia/lib/redfish/transport:http_redfish_intf",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/magent/daemons:magent_hdr",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
    ],
)

cc_test(
    name = "fake_redfish_server_test",
    srcs = ["fake_redfish_server_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/indus_hmb_cn:mockup.shar",
    ],
    deps = [
        ":fake_redfish_server",
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
    name = "grpc_dynamic_mockup_server",
    testonly = True,
    srcs = ["grpc_dynamic_mockup_server.cc"],
    hdrs = ["grpc_dynamic_mockup_server.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:test_mockup",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_proto",
        "//ecclesia/lib/status:rpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "grpc_dynamic_mockup_server_test",
    srcs = ["grpc_dynamic_mockup_server_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":grpc_dynamic_mockup_server",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_proto",
        "//ecclesia/lib/redfish/transport:grpc",
        "//ecclesia/lib/redfish/transport:grpc_dynamic",
        "//ecclesia/lib/redfish/transport:grpc_dynamic_options",
        "//ecclesia/lib/status:rpc",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "node_topology_testing",
    testonly = True,
    srcs = ["node_topology_testing.cc"],
    hdrs = ["node_topology_testing.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish:node_topology",
        "//ecclesia/lib/redfish:types",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest",
    ],
)
