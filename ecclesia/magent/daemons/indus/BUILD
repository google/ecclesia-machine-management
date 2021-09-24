load("//ecclesia/build_defs:oss.bzl", "ecclesia_oss_static_linked_cc_binary")

licenses(["notice"])

config_setting(
    name = "external_build",
    values = {"define": "build=external"},
    visibility = ["//visibility:private"],
)

ecclesia_oss_static_linked_cc_binary(
    name = "magent_indus_oss",
    srcs = ["main.cc"],
    features = ["fully_static_link"],
    linkopts = [
        "-static-libgcc -l:libm.a",
        "-static-libstdc++ -l:libstdc++.a",
    ],
    visibility = [
        "//ecclesia:magent_library_users",
    ],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/io:ioctl",
        "//ecclesia/lib/io/pci:discovery",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/io/smbus",
        "//ecclesia/lib/io/smbus:kernel_dev",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/logging:file_globals",
        "//ecclesia/lib/smbios/indus:indus_platform_translator",
        "//ecclesia/lib/types:fixed_range_int",
        "//ecclesia/magent/daemons:magent_hdr",
        "//ecclesia/magent/lib/eeprom",
        "//ecclesia/magent/lib/ipmi",
        "//ecclesia/magent/lib/ipmi:interface_options",
        "//ecclesia/magent/lib/ipmi:ipmitool",
        "//ecclesia/magent/redfish/indus",
        "//ecclesia/magent/sysmodel/indus:indus_nvme",
        "//ecclesia/magent/sysmodel/x86:sysmodel_fru",
        "//ecclesia/magent/sysmodel/x86:thermal",
        "//ecclesia/magent/sysmodel/x86:x86_sysmodel",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
    ],
)
