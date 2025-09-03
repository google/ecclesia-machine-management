# load("@rules_cc//cc:cc_library.bzl", "cc_library")
# load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "complexity_tracker",
    srcs = ["complexity_tracker.cc"],
    hdrs = ["complexity_tracker.h"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "complexity_tracker_test",
    srcs = ["complexity_tracker_test.cc"],
    deps = [
        ":complexity_tracker",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
