licenses(["notice"])

cc_library(
    name = "thermal",
    hdrs = ["thermal.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "fru",
    hdrs = ["fru.h"],
    deps = [
        "//ecclesia/magent/lib/eeprom",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
