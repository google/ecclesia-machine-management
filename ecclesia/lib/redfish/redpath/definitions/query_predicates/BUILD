cc_library(
    name = "predicates",
    srcs = ["predicates.cc"],
    hdrs = ["predicates.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/utils:path_util",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_test(
    name = "predicates_test",
    srcs = ["predicates_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":predicates",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_library(
    name = "filter",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":filter",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
