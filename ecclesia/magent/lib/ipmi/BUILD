# IPMI interface libraries for management agent

licenses(["notice"])

cc_library(
    name = "ipmi",
    hdrs = ["ipmi.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "interface_options",
    srcs = ["interface_options.cc"],
    hdrs = ["interface_options.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/magent/proto:magent_config_cc_proto",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "ipmitool",
    srcs = [
        "ipmitool.cc",
    ],
    hdrs = [
        "ipmitool.h",
        "ipmitool_interface.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":ipmi",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/status:macros",
        "//ecclesia/magent/lib/fru:ipmi_fru",
        "//ecclesia/magent/proto:magent_config_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@ipmitool//:ipmi",
    ],
)

cc_library(
    name = "ipmi_mock",
    testonly = True,
    hdrs = ["ipmi_mock.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":ipmi",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
