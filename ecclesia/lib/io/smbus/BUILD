licenses(["notice"])

cc_library(
    name = "smbus",
    srcs = ["smbus.cc"],
    hdrs = ["smbus.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "smbus_test",
    size = "small",
    srcs = ["smbus_test.cc"],
    deps = [
        ":smbus",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = True,
    hdrs = ["mocks.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":smbus",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "kernel_dev",
    srcs = ["kernel_dev.cc"],
    hdrs = ["kernel_dev.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":smbus",
        "//ecclesia/lib/io:ioctl",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "kernel_dev_test",
    size = "small",
    srcs = ["kernel_dev_test.cc"],
    deps = [
        ":kernel_dev",
        ":smbus",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/io:ioctl",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "proto",
    srcs = ["smbus.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "cc_proto",
    visibility = ["//visibility:public"],
    deps = [":proto"],
)
