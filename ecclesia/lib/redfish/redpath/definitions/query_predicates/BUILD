cc_library(
    name = "predicates",
    srcs = ["predicates.cc"],
    hdrs = ["predicates.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":util",
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
        ":predicate_object",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
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

cc_library(
    name = "predicate_object",
    srcs = ["predicate_object.cc"],
    hdrs = ["predicate_object.h"],
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
    name = "predicate_object_test",
    srcs = ["predicate_object_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":predicate_object",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "variable_substitution",
    srcs = ["variable_substitution.cc"],
    hdrs = ["variable_substitution.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":predicate_object",
        "//ecclesia/lib/redfish/dellicius/query:query_variables_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "util_test",
    srcs = ["util_test.cc"],
    deps = [
        ":util",
        "//ecclesia/lib/testing:status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "variable_substitution_test",
    srcs = ["variable_substitution_test.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":variable_substitution",
        "//ecclesia/lib/redfish/dellicius/query:query_variables_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
