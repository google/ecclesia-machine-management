# Description:
#   Helpful utilities for implementing caching systems.

package(default_applicable_licenses = [""])

licenses(["notice"])

cc_library(
    name = "rcu",
    hdrs = [
        "rcu_snapshot.h",
        "rcu_store.h",
        "rcu_view.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "rcu_snapshot_test",
    srcs = ["rcu_snapshot_test.cc"],
    deps = [
        ":rcu",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "rcu_store_test",
    srcs = ["rcu_store_test.cc"],
    deps = [
        ":rcu",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "rcu_view_test",
    srcs = ["rcu_view_test.cc"],
    deps = [
        ":rcu",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "timed_store",
    hdrs = ["timed_store.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":rcu",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "timed_store_test",
    srcs = ["timed_store_test.cc"],
    deps = [
        ":rcu",
        ":timed_store",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)
