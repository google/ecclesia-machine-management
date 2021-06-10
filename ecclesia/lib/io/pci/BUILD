licenses(["notice"])

cc_library(
    name = "discovery",
    hdrs = ["discovery.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":location",
        ":pci",
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "mocks",
    testonly = True,
    hdrs = ["mocks.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":config",
        ":discovery",
        ":location",
        ":pci",
        ":region",
        "//ecclesia/lib/codec:bits",
        "//ecclesia/lib/codec:endian",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "region",
    hdrs = ["region.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "location",
    srcs = ["location.cc"],
    hdrs = ["location.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "location_test",
    size = "small",
    srcs = ["location_test.cc"],
    deps = [
        ":location",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "signature",
    hdrs = ["signature.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = [
        "config.h",
        "registers.h",
    ],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":location",
        ":region",
        ":signature",
        "//ecclesia/lib/codec:bits",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "pci",
    hdrs = ["pci.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":config",
        ":location",
        ":region",
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/status:statusor",
    ],
)

proto_library(
    name = "proto",
    srcs = ["pci.proto"],
    visibility = ["//ecclesia:library_users"],
)

cc_proto_library(
    name = "cc_proto",
    visibility = ["//ecclesia:library_users"],
    deps = [":proto"],
)

cc_library(
    name = "proto_lib",
    srcs = ["proto_lib.cc"],
    hdrs = ["proto_lib.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":cc_proto",
        ":location",
        ":signature",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "proto_lib_test",
    srcs = ["proto_lib_test.cc"],
    deps = [
        ":cc_proto",
        ":location",
        ":proto_lib",
        ":signature",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:proto",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sysfs",
    srcs = ["sysfs.cc"],
    hdrs = ["sysfs.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":config",
        ":discovery",
        ":location",
        ":pci",
        ":region",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/file:dir",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/types:fixed_range_int",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "sysfs_test",
    size = "small",
    srcs = ["sysfs_test.cc"],
    deps = [
        ":config",
        ":discovery",
        ":location",
        ":pci",
        ":sysfs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
