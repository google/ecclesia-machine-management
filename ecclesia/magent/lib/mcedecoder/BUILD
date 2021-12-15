# Description:
# Library to perform minimal machine check exception decoding

licenses(["notice"])

cc_library(
    name = "cpu_topology",
    hdrs = ["cpu_topology.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = ["@com_google_absl//absl/status:statusor"],
)

cc_library(
    name = "dimm_translator_interface",
    hdrs = ["dimm_translator.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = ["@com_google_absl//absl/status:statusor"],
)

cc_library(
    name = "mce_messages",
    hdrs = ["mce_messages.h"],
    visibility = ["//ecclesia:magent_library_users"],
)

cc_library(
    name = "mce_decode",
    srcs = ["mce_decode.cc"],
    hdrs = ["mce_decode.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":cpu_topology",
        ":dimm_translator_interface",
        ":mce_messages",
        ":skylake_mce_decode",
        "//ecclesia/lib/codec:bits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "skylake_mce_decode",
    srcs = ["skylake_mce_decode.cc"],
    hdrs = ["skylake_mce_decode.h"],
    deps = [
        ":dimm_translator_interface",
        ":mce_messages",
        "//ecclesia/lib/codec:bits",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "cpu_topology_mock",
    testonly = True,
    hdrs = ["cpu_topology_mock.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":cpu_topology",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "mce_decode_mock",
    testonly = True,
    hdrs = ["mce_decode_mock.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":mce_decode",
        ":mce_messages",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
