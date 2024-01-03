licenses(["notice"])

cc_library(
    name = "subscription",
    srcs = ["subscription.cc"],
    hdrs = ["subscription.h"],
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
