load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

licenses(["notice"])

cc_binary(
    name = "cc_embed",
    srcs = ["cc_embed.cc"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":path",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "cc_embed_interface",
    hdrs = ["cc_embed_interface.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_data_library(
    name = "text_files",
    cc_namespace = "ecclesia",
    data = ["test_data/text.txt"],
    var_name = "kTextFiles",
)

cc_data_library(
    name = "all_files",
    cc_namespace = "ecclesia_testdata",
    data = [
        "test_data/blob.bin",
        "test_data/text.txt",
    ],
    var_name = "kAllFiles",
)

cc_data_library(
    name = "flat_files",
    cc_namespace = "ecclesia",
    data = [
        "test_data/blob.bin",
        "test_data/text.txt",
    ],
    flatten = True,
    var_name = "kFlatFiles",
)

cc_test(
    name = "cc_embed_interface_test",
    srcs = ["cc_embed_interface_test.cc"],
    deps = [
        ":all_files",
        ":cc_embed_interface",
        ":flat_files",
        ":text_files",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "dir",
    srcs = ["dir.cc"],
    hdrs = ["dir.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":path",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "dir_test",
    srcs = ["dir_test.cc"],
    deps = [
        ":dir",
        ":test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "lockfile",
    srcs = ["lockfile.cc"],
    hdrs = ["lockfile.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/status:posix",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "lockfile_test",
    srcs = ["lockfile_test.cc"],
    deps = [
        ":lockfile",
        ":path",
        ":test_filesystem",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mmap",
    srcs = ["mmap.cc"],
    hdrs = ["mmap.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "mmap_test",
    srcs = ["mmap_test.cc"],
    deps = [
        ":mmap",
        ":test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_data_library(
    name = "json_files",
    cc_namespace = "ecclesia_testdata",
    data = [
        "test_data/json.json",
        "test_data/json_bad.txt",
    ],
    var_name = "kJsonFiles",
)

cc_library(
    name = "parse_json",
    hdrs = ["parse_json.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":cc_embed_interface",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_jsoncpp//:json",
    ],
)

cc_test(
    name = "parse_json_test",
    srcs = ["parse_json_test.cc"],
    deps = [
        ":cc_embed_interface",
        ":json_files",
        ":parse_json",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_jsoncpp//:json",
    ],
)

cc_library(
    name = "path",
    srcs = ["path.cc"],
    hdrs = ["path.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "path_test",
    srcs = ["path_test.cc"],
    deps = [
        ":path",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_filesystem",
    testonly = True,
    srcs = ["test_filesystem.cc"],
    hdrs = ["test_filesystem.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":dir",
        ":path",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "test_filesystem_test",
    srcs = ["test_filesystem_test.cc"],
    deps = [
        ":path",
        ":test_filesystem",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "uds",
    srcs = ["uds.cc"],
    hdrs = ["uds.h"],
    visibility = ["//ecclesia:library_users"],
)

cc_test(
    name = "uds_test",
    srcs = ["uds_test.cc"],
    deps = [
        ":test_filesystem",
        ":uds",
        "@com_google_googletest//:gtest_main",
    ],
)
