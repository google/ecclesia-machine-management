# Description:
#   Implements a circlularbuffer

package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

cc_test(
    name = "circularbuffer_test",
    srcs = ["circularbuffer_test.cc"],
    deps = [
        ":circularbuffer",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "circularbuffer",
    hdrs = ["circularbuffer.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
    ],
)
