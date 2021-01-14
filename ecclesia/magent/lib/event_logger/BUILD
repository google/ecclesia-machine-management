licenses(["notice"])

cc_library(
    name = "event_logger",
    srcs = ["event_logger.cc"],
    hdrs = ["event_logger.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/time:clock",
        "//ecclesia/magent/lib/event_reader",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_test(
    name = "event_logger_test",
    srcs = ["event_logger_test.cc"],
    deps = [
        ":event_logger",
        "//ecclesia/lib/time:clock",
        "//ecclesia/magent/lib/event_reader",
        "//ecclesia/magent/lib/event_reader:elog_emb",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "system_event_visitors",
    srcs = ["system_event_visitors.cc"],
    hdrs = ["system_event_visitors.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        ":event_logger",
        "//ecclesia/lib/mcedecoder:mce_decode",
        "//ecclesia/lib/mcedecoder:mce_messages",
        "//ecclesia/magent/lib/event_reader",
        "//ecclesia/magent/lib/event_reader:elog_emb",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

cc_test(
    name = "system_event_visitors_test",
    srcs = ["system_event_visitors_test.cc"],
    deps = [
        ":event_logger",
        ":system_event_visitors",
        "//ecclesia/lib/mcedecoder:mce_decode_mock",
        "//ecclesia/lib/mcedecoder:mce_messages",
        "//ecclesia/lib/time:clock",
        "//ecclesia/magent/lib/event_reader",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:optional",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "intel_cpu_topology",
    srcs = ["intel_cpu_topology.cc"],
    hdrs = ["intel_cpu_topology.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/mcedecoder:cpu_topology",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "intel_cpu_topology_test",
    size = "small",
    srcs = ["intel_cpu_topology_test.cc"],
    deps = [
        "intel_cpu_topology",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)
