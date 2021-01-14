licenses(["notice"])

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
