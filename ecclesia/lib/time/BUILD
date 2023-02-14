package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

cc_library(
    name = "clock",
    srcs = ["clock.cc"],
    hdrs = ["clock.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/time"],
)

cc_library(
    name = "clock_fake",
    testonly = True,
    hdrs = ["clock_fake.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":clock",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "proto",
    srcs = ["proto.cc"],
    hdrs = ["proto.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "proto_test",
    srcs = ["proto_test.cc"],
    deps = [
        ":proto",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
