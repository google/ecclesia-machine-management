licenses(["notice"])

cc_library(
    name = "hosts",
    srcs = ["hosts.cc"],
    hdrs = ["hosts.h"],
    visibility = ["//ecclesia:library_users"],
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
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/cleanup",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
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
