# load("@rules_cc//cc:cc_library.bzl", "cc_library")
# load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "arbiter",
    hdrs = ["arbiter.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "arbiter_test",
    srcs = ["arbiter_test.cc"],
    deps = [
        ":arbiter",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/thread",
        "//ecclesia/lib/time:clock",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":arbiter",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
    ],
)

cc_test(
    name = "util_test",
    srcs = ["util_test.cc"],
    deps = [
        ":arbiter",
        ":util",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
