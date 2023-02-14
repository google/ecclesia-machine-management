package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

cc_library(
    name = "task",
    hdrs = ["task.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/time"],
)

cc_test(
    name = "task_test",
    srcs = ["task_test.cc"],
    deps = [
        ":task",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "manager",
    hdrs = ["manager.h"],
    visibility = ["//visibility:public"],
    deps = [":task"],
)

cc_test(
    name = "manager_test",
    srcs = ["manager_test.cc"],
    deps = [
        ":manager",
        ":task",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "task_wrapper_manager",
    testonly = True,
    hdrs = ["task_wrapper_manager.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":manager",
        ":task",
    ],
)
