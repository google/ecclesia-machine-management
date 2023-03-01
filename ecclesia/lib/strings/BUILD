# Ecclesia library for handling strings

package(default_applicable_licenses = [""])

licenses(["notice"])

cc_library(
    name = "natural_sort",
    srcs = ["natural_sort.cc"],
    hdrs = ["natural_sort.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "regex",
    hdrs = ["regex.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "string_view",
    hdrs = ["string_view.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "strip",
    srcs = ["strip.cc"],
    hdrs = ["strip.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "case",
    srcs = ["case.cc"],
    hdrs = ["case.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "case_test",
    srcs = ["case_test.cc"],
    deps = [
        ":case",
        "@com_google_googletest//:gtest_main",
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
        "@com_google_googletest//:gtest_main",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "string_view_test",
    srcs = ["string_view_test.cc"],
    deps = [
        ":string_view",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "strip_test",
    size = "small",
    timeout = "moderate",
    srcs = ["strip_test.cc"],
    deps = [
        ":strip",
        "@com_google_googletest//:gtest_main",
    ],
)
