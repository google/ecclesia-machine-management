licenses(["notice"])

cc_library(
    name = "subscription",
    srcs = ["subscription.cc"],
    hdrs = ["subscription.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
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
    name = "susbscription_impl",
    srcs = ["susbscription_impl.cc"],
    hdrs = ["subscription_impl.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":subscription",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
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
    name = "subscription_mock",
    testonly = 1,
    hdrs = ["subscription_mock.h"],
    deps = [
        ":subscription",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest",
        "@com_json//:json",
    ],
)

cc_test(
    name = "subscription_test",
    srcs = ["subscription_test.cc"],
    deps = [
        ":subscription",
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
        ":subscription_mock",
        ":susbscription_impl",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)