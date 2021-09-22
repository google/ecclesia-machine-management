# Description:
#   Libraries to provide input / output.

licenses(["notice"])

cc_library(
    name = "generic_mmio",
    srcs = ["generic_mmio.cc"],
    hdrs = ["generic_mmio.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/codec:bits",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/file:mmap",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "generic_mmio_test",
    size = "small",
    srcs = ["generic_mmio_test.cc"],
    deps = [
        ":generic_mmio",
        "//ecclesia/lib/codec:bits",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ioctl",
    srcs = ["ioctl.cc"],
    hdrs = ["ioctl.h"],
    visibility = ["//ecclesia:library_users"],
)

cc_library(
    name = "msr",
    srcs = ["msr.cc"],
    hdrs = [
        "constants.h",
        "msr.h",
    ],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "mocks",
    testonly = True,
    hdrs = ["mocks.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":generic_mmio",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "msr_test",
    size = "small",
    srcs = ["msr_test.cc"],
    deps = [
        ":msr",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_googletest//:gtest_main",
    ],
)
