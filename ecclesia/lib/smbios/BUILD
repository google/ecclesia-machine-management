load("@com_google_emboss//:build_defs.bzl", "emboss_cc_library")

licenses(["notice"])

emboss_cc_library(
    name = "entry_point_emb",
    srcs = ["entry_point.emb"],
)

emboss_cc_library(
    name = "structures_emb",
    srcs = ["structures.emb"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "platform_translator",
    hdrs = [
        "platform_translator.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "reader",
    srcs = [
        "processor_information.cc",
        "reader.cc",
    ],
    hdrs = [
        "baseboard_information.h",
        "bios.h",
        "internal.h",
        "memory_device.h",
        "memory_device_mapped_address.h",
        "platform_translator.h",
        "processor_information.h",
        "reader.h",
        "system_event_log.h",
        "system_information.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":entry_point_emb",
        ":structures_emb",
        "//ecclesia/lib/strings:natural_sort",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)
