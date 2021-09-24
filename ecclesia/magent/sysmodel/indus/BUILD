# This package comtains some sysmodel libraries that are specific to Indus platform.

licenses(["notice"])

cc_library(
    name = "indus_nvme",
    srcs = ["nvme.cc"],
    hdrs = ["nvme.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/io/pci",
        "//ecclesia/lib/io/pci:config",
        "//ecclesia/lib/io/pci:discovery",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/magent/sysmodel/x86:nvme",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "nvme_test",
    srcs = ["nvme_test.cc"],
    deps = [
        ":indus_nvme",
        "//ecclesia/lib/io/pci:config",
        "//ecclesia/lib/io/pci:discovery",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/io/pci:mocks",
        "//ecclesia/magent/sysmodel/x86:nvme",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
