# Description:
#   Various libraries for accessing low-level buses and devices present
#   on platforms hardware.

licenses(["notice"])

cc_library(
    name = "smbus",
    hdrs = ["smbus.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "smbus_mocks",
    testonly = True,
    hdrs = ["smbus_mocks.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":smbus",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "smbus_kernel_dev",
    srcs = ["smbus_kernel_dev.cc"],
    hdrs = ["smbus_kernel_dev.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":smbus",
        "//ecclesia/lib/cleanup",
        "//ecclesia/lib/io:ioctl",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "smbus_test",
    size = "small",
    srcs = ["smbus_test.cc"],
    deps = [
        ":smbus",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "smbus_kernel_dev_test",
    size = "small",
    srcs = ["smbus_kernel_dev_test.cc"],
    deps = [
        ":smbus",
        ":smbus_kernel_dev",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/io:ioctl",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "usb",
    srcs = [
        "usb.cc",
    ],
    hdrs = [
        "usb.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "usb_sysfs",
    srcs = [
        "usb_sysfs.cc",
    ],
    hdrs = [
        "usb_sysfs.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":usb",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "usb_mock",
    testonly = True,
    hdrs = ["usb_mock.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":usb",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "usb_ids",
    hdrs = ["usb_ids.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [":usb"],
)

cc_test(
    name = "usb_test",
    size = "small",
    srcs = ["usb_test.cc"],
    deps = [
        ":usb",
        ":usb_mock",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "usb_sysfs_test",
    size = "small",
    srcs = ["usb_sysfs_test.cc"],
    deps = [
        ":usb",
        ":usb_sysfs",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)
