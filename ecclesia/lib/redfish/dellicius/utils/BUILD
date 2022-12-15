cc_library(
    name = "path_util",
    srcs = ["path_util.cc"],
    hdrs = ["path_util.h"],
    visibility = [
        "//ecclesia/lib/redfish/dellicius:__subpackages__",
    ],
    deps = [
        "//ecclesia/lib/redfish:interface",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "path_util_test",
    srcs = ["path_util_test.cc"],
    deps = [
        ":path_util",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
