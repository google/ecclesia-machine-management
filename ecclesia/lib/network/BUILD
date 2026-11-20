# load("@rules_cc//cc:cc_library.bzl", "cc_library")
# load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "hosts",
    srcs = ["hosts.cc"],
    hdrs = ["hosts.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "hosts_test",
    srcs = ["hosts_test.cc"],
    deps = [
        ":hosts",
        "//ecclesia/lib/file:test_filesystem",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ip",
    srcs = ["ip.cc"],
    hdrs = ["ip.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/cleanup"],
)

cc_test(
    name = "ip_test",
    srcs = ["ip_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":ip",
        "@com_google_googletest//:gtest_main",
    ],
)

# :testing without testonly, approved for a select few users.
cc_library(
    name = "testing_internal",
    srcs = ["testing.cc"],
    hdrs = ["testing.h"],
    visibility = [
        "//ecclesia/lib/redfish:__pkg__",
    ],
    deps = [
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
    ],
)

cc_library(
    name = "testing",
    testonly = True,
    hdrs = ["testing.h"],
    visibility = ["//visibility:public"],
    deps = [":testing_internal"],
)

cc_test(
    name = "testing_test",
    srcs = ["testing_test.cc"],
    deps = [
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)
