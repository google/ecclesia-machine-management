# Description:
#   Utilities for doing various tasks and functions involving atomic operations
#   or basic synchronization and sequencing.

package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

cc_library(
    name = "sequence",
    hdrs = ["sequence.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "sequence_test",
    srcs = ["sequence_test.cc"],
    deps = [
        ":sequence",
        "@com_google_googletest//:gtest_main",
    ],
)
