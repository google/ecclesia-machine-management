licenses(["notice"])

cc_library(
    name = "complexity_tracker",
    srcs = ["complexity_tracker.cc"],
    hdrs = ["complexity_tracker.h"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
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
