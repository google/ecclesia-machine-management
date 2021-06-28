# A http library that wraps around curl.

licenses(["notice"])

cc_library(
    name = "client",
    srcs = ["client.cc"],
    hdrs = ["client.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_jsoncpp//:json",
    ],
)

cc_test(
    name = "client_test",
    size = "small",
    srcs = ["client_test.cc"],
    deps = [
        ":client",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_jsoncpp//:json",
    ],
)

cc_library(
    name = "curl_client",
    srcs = ["curl_client.cc"],
    hdrs = ["curl_client.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":client",
        ":cred_cc_proto",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_jsoncpp//:json",
        "@curl",
    ],
)

cc_test(
    name = "curl_client_test",
    size = "small",
    srcs = ["curl_client_test.cc"],
    deps = [
        ":curl_client",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "cred_proto",
    srcs = ["cred.proto"],
    visibility = ["//ecclesia:library_users"],
)

cc_proto_library(
    name = "cred_cc_proto",
    visibility = ["//ecclesia:library_users"],
    deps = [":cred_proto"],
)
