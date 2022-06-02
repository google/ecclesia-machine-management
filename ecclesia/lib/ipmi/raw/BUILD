# The Raw IPMI modules.  Each is an implementation of just sending an IPMI
# packet without adding value to it.

licenses(["notice"])

cc_library(
    name = "ipmitool",
    srcs = ["ipmitool.cc"],
    hdrs = ["ipmitool.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/ipmi:ipmi_consts",
        "//ecclesia/lib/ipmi:ipmi_raw_interface",
        "//ecclesia/lib/ipmi:ipmi_request",
        "//ecclesia/lib/ipmi:ipmi_response",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@ipmitool//:ipmi",
    ],
)

cc_test(
    name = "ipmitool_test",
    srcs = ["ipmitool_test.cc"],
    deps = [
        ":ipmitool",
        "//ecclesia/lib/ipmi:ipmi_consts",
        "//ecclesia/lib/ipmi:ipmi_pair",
        "//ecclesia/lib/ipmi:ipmi_request",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
        "@ipmitool//:ipmi",
    ],
)
