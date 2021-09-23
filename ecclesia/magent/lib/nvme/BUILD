load("@com_google_emboss//:build_defs.bzl", "emboss_cc_library")

licenses(["notice"])

cc_library(
    name = "libnvme",
    srcs = [
        "identify_controller.cc",
        "identify_namespace.cc",
        "nvme_device.cc",
        "nvme_linux_access.cc",
    ],
    hdrs = [
        "device_self_test_log.h",
        "firmware_slot_info.h",
        "identify_controller.h",
        "identify_namespace.h",
        "nvme_access.h",
        "nvme_device.h",
        "nvme_linux_access.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":controller_registers",
        ":nvme_types_emb",
        ":sanitize_log_page",
        ":smart_log_page",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/file:mmap",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_library(
    name = "mock_nvme_device",
    testonly = 1,
    hdrs = ["mock_nvme_device.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":controller_registers",
        ":libnvme",
        ":nvme_types_emb",
        ":sanitize_log_page",
        ":smart_log_page",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "smart_log_page",
    srcs = ["smart_log_page.cc"],
    hdrs = ["smart_log_page.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":nvme_types_emb",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/logging",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

cc_library(
    name = "sanitize_log_page",
    srcs = ["sanitize_log_page.cc"],
    hdrs = ["sanitize_log_page.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":nvme_types_emb",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

emboss_cc_library(
    name = "nvme_types_emb",
    srcs = ["nvme_types.emb"],
)

cc_test(
    name = "identify_controller_test",
    size = "small",
    srcs = ["identify_controller_test.cc"],
    deps = [
        ":libnvme",
        ":nvme_types_emb",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "smart_log_page_test",
    size = "small",
    srcs = ["smart_log_page_test.cc"],
    deps = [
        ":smart_log_page",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "sanitize_log_page_test",
    size = "small",
    srcs = ["sanitize_log_page_test.cc"],
    deps = [
        ":nvme_types_emb",
        ":sanitize_log_page",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "identify_namespace_test",
    srcs = ["identify_namespace_test.cc"],
    deps = [
        ":libnvme",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "firmware_slot_info_test",
    size = "small",
    srcs = ["firmware_slot_info_test.cc"],
    deps = [
        ":libnvme",
        ":nvme_types_emb",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "device_self_test_log_test",
    size = "small",
    srcs = ["device_self_test_log_test.cc"],
    deps = [
        ":libnvme",
        ":nvme_types_emb",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nvme_device_test",
    srcs = ["nvme_device_test.cc"],
    deps = [
        ":controller_registers",
        ":libnvme",
        ":mock_nvme_device",
        ":nvme_types_emb",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

emboss_cc_library(
    name = "controller_registers_emb",
    srcs = ["controller_registers.emb"],
    visibility = ["//ecclesia:magent_library_users"],
)

cc_library(
    name = "controller_registers",
    srcs = [],
    hdrs = ["controller_registers.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":controller_registers_emb",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "controller_registers_test",
    size = "small",
    srcs = ["controller_registers_test.cc"],
    deps = [
        ":controller_registers",
        ":controller_registers_emb",
        "@com_google_absl//absl/status:statusor",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)
