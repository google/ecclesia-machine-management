# Description:
#   Libraries to simplify access to API filesystems (e.g. procfs, sysfs).

licenses(["notice"])

cc_library(
    name = "apifs",
    srcs = ["apifs.cc"],
    hdrs = ["apifs.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/cleanup",
        "//ecclesia/lib/file:dir",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "apifs_test",
    size = "small",
    srcs = ["apifs_test.cc"],
    deps = [
        ":apifs",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
