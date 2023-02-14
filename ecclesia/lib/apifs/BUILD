# Description:
#   Libraries to simplify access to API filesystems (e.g. procfs, sysfs).

package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

cc_library(
    name = "apifs",
    srcs = ["apifs.cc"],
    hdrs = ["apifs.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/file:dir",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/cleanup",
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

cc_binary(
    name = "apifs_benchmark",
    srcs = ["apifs_benchmark.cc"],
    linkstatic = True,
    deps = [
        ":apifs",
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
