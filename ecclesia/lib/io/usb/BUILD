licenses(["notice"])

cc_library(
    name = "usb",
    srcs = ["usb.cc"],
    hdrs = ["usb.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "usb_test",
    size = "small",
    srcs = ["usb_test.cc"],
    deps = [
        ":mocks",
        ":usb",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mocks",
    testonly = True,
    hdrs = ["mocks.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":usb",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ids",
    hdrs = ["ids.h"],
    visibility = ["//visibility:public"],
    deps = [":usb"],
)

cc_library(
    name = "sysfs",
    srcs = ["sysfs.cc"],
    hdrs = ["sysfs.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":usb",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "proto_lib",
    srcs = ["proto_lib.cc"],
    hdrs = ["proto_lib.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":cc_proto",
        ":usb",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "sysfs_test",
    size = "small",
    srcs = ["sysfs_test.cc"],
    deps = [
        ":sysfs",
        ":usb",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "proto_lib_test",
    srcs = ["proto_lib_test.cc"],
    deps = [
        ":cc_proto",
        ":proto_lib",
        ":usb",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "proto",
    srcs = ["usb.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "cc_proto",
    visibility = ["//visibility:public"],
    deps = [":proto"],
)
