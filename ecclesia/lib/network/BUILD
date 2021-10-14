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
    name = "testing",
    testonly = True,
    srcs = ["testing.cc"],
    hdrs = ["testing.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "@com_google_absl//absl/cleanup",
    ],
)

cc_test(
    name = "testing_test",
    srcs = ["testing_test.cc"],
    deps = [
        ":testing",
        "@com_google_googletest//:gtest_main",
    ],
)
