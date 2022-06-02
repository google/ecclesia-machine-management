licenses(["notice"])

cc_library(
    name = "ipmi_mechanisms",
    hdrs = ["ipmi_mechanisms.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ipmi_consts",
    hdrs = ["ipmi_consts.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "kcs",
    hdrs = ["kcs.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_consts",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "ipmi_transport_consts",
    hdrs = ["ipmi_transport_consts.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ipmi_response",
    hdrs = ["ipmi_response.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_consts",
    ],
)

cc_library(
    name = "ipmi_stats",
    hdrs = ["ipmi_stats.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_command_identifier",
        "//ecclesia/lib/circularbuffer",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "ipmi_command_identifier",
    hdrs = ["ipmi_command_identifier.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ipmi_handle_mock",
    testonly = 1,
    hdrs = ["ipmi_handle_mock.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_handle",
        ":ipmi_response",
        "//ecclesia/lib/ipmi/raw:ipmitool",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "ipmitool_mock",
    testonly = 1,
    hdrs = ["ipmitool_mock.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ipmi_handle",
    srcs = ["ipmi_handle.cc"],
    hdrs = ["ipmi_handle.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_raw_interface",
        ":ipmi_request",
        ":ipmi_response",
        "//ecclesia/lib/arbiter",
        "//ecclesia/lib/ipmi/raw:ipmitool",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@ipmitool//:ipmi",
    ],
)

cc_library(
    name = "ipmi_manager",
    srcs = ["ipmi_manager.cc"],
    hdrs = ["ipmi_manager.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_consts",
        ":ipmi_handle",
        ":ipmi_interface_params_cc_proto",
        "//ecclesia/lib/arbiter",
        "//ecclesia/lib/ipmi/raw:ipmitool",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/thread",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@ipmitool//:ipmi",
    ],
)

cc_library(
    name = "ipmi_manager_mock",
    testonly = 1,
    hdrs = ["ipmi_manager_mock.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_handle",
        ":ipmi_interface_params_cc_proto",
        ":ipmi_manager",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "ipmi_pair",
    srcs = ["ipmi_pair.cc"],
    hdrs = ["ipmi_pair.h"],
    visibility = ["//visibility:public"],
    deps = [":ipmi_consts"],
)

cc_library(
    name = "ipmi_request",
    hdrs = ["ipmi_request.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_consts",
        ":ipmi_pair",
        "@com_google_absl//absl/types:span",
    ],
)

proto_library(
    name = "ipmi_interface_params_proto",
    srcs = ["ipmi_interface_params.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "ipmi_interface_params_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":ipmi_interface_params_proto"],
)

cc_test(
    name = "ipmi_manager_test",
    srcs = ["ipmi_manager_test.cc"],
    deps = [
        ":ipmi_handle",
        ":ipmi_interface_params_cc_proto",
        ":ipmi_manager",
        ":ipmitool_mock",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:clock",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@ipmitool//:ipmi",
    ],
)

cc_library(
    name = "ipmi_raw_interface",
    hdrs = ["ipmi_raw_interface.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_request",
        ":ipmi_response",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "ipmi_raw_mock",
    testonly = 1,
    hdrs = ["ipmi_raw_mock.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":ipmi_raw_interface",
        ":ipmi_request",
        ":ipmi_response",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest",
    ],
)
