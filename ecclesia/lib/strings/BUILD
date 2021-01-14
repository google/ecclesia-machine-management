# Ecclesia library for handling strings

licenses(["notice"])

cc_library(
    name = "natural_sort",
    srcs = ["natural_sort.cc"],
    hdrs = ["natural_sort.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "regex",
    hdrs = ["regex.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "natural_sort_test",
    size = "small",
    timeout = "moderate",
    srcs = ["natural_sort_test.cc"],
    deps = [
        ":natural_sort",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "regex_test",
    size = "small",
    timeout = "moderate",
    srcs = ["regex_test.cc"],
    deps = [
        ":regex",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
