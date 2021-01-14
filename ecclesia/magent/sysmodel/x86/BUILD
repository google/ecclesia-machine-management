licenses(["notice"])

cc_library(
    name = "x86_sysmodel",
    srcs = ["sysmodel.cc"],
    hdrs = ["sysmodel.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":chassis",
        ":cpu",
        ":dimm",
        ":nvme",
        ":pci_storage",
        ":sysmodel_fru",
        ":thermal",
        "//ecclesia/lib/io/pci",
        "//ecclesia/lib/io/pci:config",
        "//ecclesia/lib/io/pci:discovery",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/io/pci:sysfs",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/smbios:reader",
        "//ecclesia/lib/time:clock",
        "//ecclesia/magent/lib/event_logger",
        "//ecclesia/magent/lib/event_logger:system_event_visitors",
        "//ecclesia/magent/lib/event_reader",
        "//ecclesia/magent/lib/event_reader:elog_reader",
        "//ecclesia/magent/lib/event_reader:mced_reader",
        "//ecclesia/magent/lib/io:usb",
        "//ecclesia/magent/lib/io:usb_sysfs",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "dimm",
    srcs = ["dimm.cc"],
    hdrs = ["dimm.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/smbios:reader",
        "//ecclesia/lib/smbios:structures_emb",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

cc_library(
    name = "cpu",
    srcs = ["cpu.cc"],
    hdrs = ["cpu.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/io:msr",
        "//ecclesia/lib/smbios:reader",
        "//ecclesia/lib/smbios:structures_emb",
        "//ecclesia/magent/lib/event_logger:intel_cpu_topology",
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
    name = "sysmodel_fru",
    srcs = ["fru.cc"],
    hdrs = ["fru.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/magent/lib/eeprom",
        "//ecclesia/magent/lib/fru:ipmi_fru",
        "//ecclesia/magent/lib/ipmi",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "thermal",
    srcs = ["thermal.cc"],
    hdrs = ["thermal.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/io:msr",
        "//ecclesia/lib/io/pci",
        "//ecclesia/lib/io/pci:config",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/io/pci:region",
        "//ecclesia/lib/io/pci:sysfs",
        "//ecclesia/magent/lib/event_logger:intel_cpu_topology",
        "//ecclesia/magent/sysmodel:thermal",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "chassis",
    srcs = ["chassis.cc"],
    hdrs = ["chassis.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/magent/lib/io:usb",
        "//ecclesia/magent/lib/io:usb_ids",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "nvme",
    srcs = ["nvme.cc"],
    hdrs = ["nvme.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/magent/lib/nvme:libnvme",
        "//ecclesia/magent/lib/nvme:smart_log_page",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "nvme_mock",
    testonly = True,
    hdrs = ["nvme_mock.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":nvme",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "sysmodel_fru_test",
    size = "small",
    srcs = ["fru_test.cc"],
    deps = [
        ":sysmodel_fru",
        "//ecclesia/magent/lib/eeprom",
        "//ecclesia/magent/lib/eeprom:smbus_eeprom_mock",
        "//ecclesia/magent/lib/ipmi:ipmi_mock",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "chassis_test",
    srcs = ["chassis_test.cc"],
    deps = [
        ":chassis",
        "//ecclesia/magent/lib/io:usb",
        "//ecclesia/magent/lib/io:usb_ids",
        "//ecclesia/magent/lib/io:usb_mock",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "nvme_test",
    srcs = ["nvme_test.cc"],
    deps = [
        ":nvme",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/magent/lib/nvme:libnvme",
        "//ecclesia/magent/lib/nvme:mock_nvme_device",
        "//ecclesia/magent/lib/nvme:smart_log_page",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "sysmodel_test",
    srcs = ["sysmodel_test.cc"],
    deps = [
        ":nvme",
        ":nvme_mock",
        ":sysmodel_fru",
        ":thermal",
        ":x86_sysmodel",
        "//ecclesia/lib/io/pci:discovery",
        "//ecclesia/lib/io/pci:location",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "pci_storage",
    hdrs = ["pci_storage.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/io/pci:location",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
    ],
)
