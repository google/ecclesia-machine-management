licenses(["notice"])

cc_library(
    name = "query_timeout_manager",
    hdrs = ["query_timeout_manager.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "query_timeout_manager_test",
    srcs = ["query_timeout_manager_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_timeout_manager",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/thread",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
