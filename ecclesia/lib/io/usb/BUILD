licenses(["notice"])

cc_library(
    name = "usb",
    srcs = ["usb.cc"],
    hdrs = ["usb.h"],
    visibility = ["//ecclesia:library_users"],
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

cc_test(
    name = "usb_test",
    size = "small",
    srcs = ["usb_test.cc"],
    deps = [
        ":mocks",
        ":usb",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = True,
    hdrs = ["mocks.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":usb",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ids",
    hdrs = ["ids.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [":usb"],
)

cc_library(
    name = "sysfs",
    srcs = ["sysfs.cc"],
    hdrs = ["sysfs.h"],
    visibility = ["//ecclesia:library_users"],
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

cc_test(
    name = "sysfs_test",
    size = "small",
    srcs = ["sysfs_test.cc"],
    deps = [
        ":sysfs",
        ":usb",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)
