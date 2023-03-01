package(default_applicable_licenses = [""])

licenses(["notice"])

cc_library(
    name = "devpath",
    srcs = ["devpath.cc"],
    hdrs = ["devpath.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "devpath_test",
    srcs = ["devpath_test.cc"],
    deps = [
        ":devpath",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "transform",
    srcs = ["transform.cc"],
    hdrs = ["transform.h"],
    visibility = ["//ecclesia:mmanager_middle_users"],
    deps = [
        "//ecclesia/lib/protobuf:field_mask",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

proto_library(
    name = "transform_test_proto",
    testonly = True,
    srcs = ["transform_test.proto"],
)

cc_proto_library(
    name = "transform_test_cc_proto",
    testonly = True,
    deps = [":transform_test_proto"],
)

cc_test(
    name = "transform_test",
    srcs = ["transform_test.cc"],
    deps = [
        ":transform",
        ":transform_test_cc_proto",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
