licenses(["notice"])

cc_library(
    name = "bytes",
    srcs = ["bytes.cc"],
    hdrs = ["bytes.h"],
    visibility = ["//ecclesia:library_users"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_test(
    name = "bytes_test",
    size = "small",
    timeout = "moderate",
    srcs = ["bytes_test.cc"],
    deps = [
        ":bytes",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "enum",
    hdrs = ["enum.h"],
    visibility = ["//ecclesia:library_users"],
)

cc_test(
    name = "enum_test",
    srcs = ["enum_test.cc"],
    deps = [
        ":enum",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "fixed_range_int",
    hdrs = ["fixed_range_int.h"],
    visibility = ["//ecclesia:library_users"],
)

cc_test(
    name = "fixed_range_int_test",
    srcs = ["fixed_range_int_test.cc"],
    deps = [
        ":fixed_range_int",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "overloaded",
    hdrs = ["overloaded.h"],
    visibility = ["//ecclesia:library_users"],
)
