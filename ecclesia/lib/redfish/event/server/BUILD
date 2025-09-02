# load("@rules_cc//cc:cc_library.bzl", "cc_library")
# load("@rules_cc//cc:cc_test.bzl", "cc_test")

licenses(["notice"])

cc_library(
    name = "subscription",
    srcs = ["subscription.cc"],
    hdrs = ["subscription.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_library(
    name = "subscription_impl",
    srcs = ["subscription_impl.cc"],
    hdrs = ["subscription_impl.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":subscription",
        "//ecclesia/lib/redfish/redpath/definitions/query_predicates:predicates",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:die_if_null",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_library(
    name = "subscription_store",
    srcs = ["subscription_store_impl.cc"],
    hdrs = ["subscription_store_impl.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":subscription",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
        "@com_json//:json",
    ],
)

cc_library(
    name = "subscription_mock",
    testonly = 1,
    hdrs = ["subscription_mock.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":subscription",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
        "@com_json//:json",
    ],
)

cc_library(
    name = "event_store",
    srcs = ["event_store.cc"],
    hdrs = ["event_store.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":subscription",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/synchronization",
        "@com_json//:json",
    ],
)

cc_test(
    name = "event_store_test",
    srcs = ["event_store_test.cc"],
    deps = [
        ":event_store",
        ":subscription",
        ":subscription_mock",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_test(
    name = "subscription_test",
    srcs = ["subscription_test.cc"],
    deps = [
        ":subscription",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_test(
    name = "subscription_impl_test",
    srcs = ["subscription_impl_test.cc"],
    deps = [
        ":subscription",
        ":subscription_impl",
        ":subscription_mock",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_library(
    name = "redfish_id",
    srcs = ["redfish_id.hpp"],
    hdrs = ["redfish_id.hpp"],
    deps = [
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "redfish_id_test",
    srcs = ["redfish_id_test.cc"],
    deps = [
        ":redfish_id",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "subscription_store_test",
    srcs = ["subscription_store_test.cc"],
    deps = [
        ":subscription",
        ":subscription_store",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)
