# Description:
#   Library to provide threaded access to a non-thread-safe resource

licenses(["notice"])

cc_library(
    name = "arbiter",
    hdrs = ["arbiter.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "arbiter_test",
    size = "small",
    srcs = ["arbiter_test.cc"],
    deps = [
        ":arbiter",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
