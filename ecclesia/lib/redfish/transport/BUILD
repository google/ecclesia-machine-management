# Transport interfaces for Redfish

licenses(["notice"])

cc_library(
    name = "interface",
    hdrs = ["interface.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_json//:json",
    ],
)

cc_library(
    name = "http",
    srcs = ["http.cc"],
    hdrs = ["http.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interface",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_json//:json",
    ],
)

cc_test(
    name = "http_test",
    srcs = ["http_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":http",
        ":interface",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
        "@com_json//:json",
    ],
)

cc_library(
    name = "http_redfish_intf",
    srcs = ["http_redfish_intf.cc"],
    hdrs = ["http_redfish_intf.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":http",
        ":interface",
        "//ecclesia/lib/http:codes",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:json_ptr",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_test(
    name = "http_redfish_intf_test",
    srcs = ["http_redfish_intf_test.cc"],
    data = [
        "//ecclesia/redfish_mockups/barebones_session_auth:mockup.shar",
    ],
    deps = [
        ":http",
        ":http_redfish_intf",
        ":interface",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/http:client",
        "//ecclesia/lib/http:cred_cc_proto",
        "//ecclesia/lib/http:curl_client",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/testing:fake_redfish_server",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)
