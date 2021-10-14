# Description:
#   Libraries for accessing Eeprom

licenses(["notice"])

cc_library(
    name = "eeprom",
    srcs = ["smbus_eeprom.cc"],
    hdrs = [
        "eeprom.h",
        "smbus_eeprom.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/io/smbus",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "smbus_eeprom_test",
    size = "small",
    srcs = ["smbus_eeprom_test.cc"],
    deps = [
        ":eeprom",
        "//ecclesia/lib/io/smbus",
        "//ecclesia/lib/io/smbus:mocks",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "smbus_eeprom_mock",
    testonly = True,
    hdrs = ["smbus_eeprom_mock.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":eeprom",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)
