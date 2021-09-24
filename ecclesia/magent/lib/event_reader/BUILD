load("@com_google_emboss//:build_defs.bzl", "emboss_cc_library")

licenses(["notice"])

cc_library(
    name = "event_reader",
    hdrs = ["event_reader.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":elog_emb",
        "@com_google_absl//absl/time",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

cc_library(
    name = "mced_reader",
    srcs = [
        "mced_reader.cc",
    ],
    hdrs = [
        "mced_reader.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":event_reader",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:posix",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "elog_reader",
    srcs = ["elog_reader.cc"],
    hdrs = ["elog_reader.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":elog_emb",
        ":event_reader",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/smbios:reader",
        "//ecclesia/lib/smbios:structures_emb",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

emboss_cc_library(
    name = "elog_emb",
    srcs = ["elog.emb"],
    visibility = ["//ecclesia:magent_library_users"],
)

cc_test(
    name = "mced_reader_test",
    srcs = ["mced_reader_test.cc"],
    deps = [
        ":event_reader",
        ":mced_reader",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "elog_reader_test",
    srcs = ["elog_reader_test.cc"],
    data = [
        ":test_data/elog.bin",
    ],
    deps = [
        ":elog_emb",
        ":elog_reader",
        ":event_reader",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/smbios:reader",
        "//ecclesia/lib/smbios:structures_emb",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)
