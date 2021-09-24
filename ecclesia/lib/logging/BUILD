# Description:
#   Libraries to for supporting the logging of errors and information.

licenses(["notice"])

cc_library(
    name = "logging",
    srcs = ["globals.cc"],
    hdrs = [
        "globals.h",
        "interfaces.h",
        "logging.h",
    ],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "logging_test",
    srcs = ["logging_test.cc"],
    deps = [
        ":logging",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "posix",
    srcs = ["posix.cc"],
    hdrs = ["posix.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":logging",
        ":posix",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "file_test",
    srcs = ["file_test.cc"],
    deps = [
        ":file",
        ":logging",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "file_globals",
    srcs = ["file_globals.cc"],
    hdrs = ["file_globals.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":file",
        ":logging",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)
