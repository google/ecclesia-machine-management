# Description:
#   Libraries to for supporting the logging of errors and information.

package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

cc_library(
    name = "logging",
    srcs = [],
    hdrs = [
        "interfaces.h",
    ],
    visibility = ["//visibility:public"],
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
