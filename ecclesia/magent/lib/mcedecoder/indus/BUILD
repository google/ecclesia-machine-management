# Description:
# Library to perform minimal machine check exception decoding

licenses(["notice"])

cc_library(
    name = "indus_dimm_translator",
    srcs = ["dimm_translator.cc"],
    hdrs = ["dimm_translator.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/magent/lib/mcedecoder:dimm_translator_interface",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "mce_decode_test",
    srcs = ["mce_decode_test.cc"],
    deps = [
        ":indus_dimm_translator",
        "//ecclesia/magent/lib/mcedecoder:cpu_topology",
        "//ecclesia/magent/lib/mcedecoder:cpu_topology_mock",
        "//ecclesia/magent/lib/mcedecoder:dimm_translator_interface",
        "//ecclesia/magent/lib/mcedecoder:mce_decode",
        "//ecclesia/magent/lib/mcedecoder:mce_messages",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
