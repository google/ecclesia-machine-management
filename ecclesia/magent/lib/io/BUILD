# Description:
#   Various libraries for accessing low-level buses and devices present
#   on platforms hardware.

licenses(["notice"])

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
