licenses(["notice"])

cc_library(
    name = "field_mask",
    srcs = ["field_mask.cc"],
    hdrs = ["field_mask.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "parse",
    hdrs = ["parse.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)
