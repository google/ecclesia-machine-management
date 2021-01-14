# Description:
#   A generic library for parsing and generating FRUs.

load("@com_google_emboss//:build_defs.bzl", "emboss_cc_library")

licenses(["notice"])

emboss_cc_library(
    name = "common_header_emb",
    srcs = ["common_header.emb"],
)

cc_library(
    name = "ipmi_fru",
    srcs = ["fru.cc"],
    hdrs = ["fru.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":common_header_emb",
        "//ecclesia/lib/codec:text",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

cc_test(
    name = "fru_test",
    size = "small",
    srcs = ["fru_test.cc"],
    deps = [
        ":common_header_emb",
        ":ipmi_fru",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)
