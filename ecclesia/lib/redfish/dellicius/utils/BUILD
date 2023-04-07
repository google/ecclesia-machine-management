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
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "path_util_test",
    srcs = ["path_util_test.cc"],
    deps = [
        ":path_util",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "id_mapper",
    hdrs = ["id_mapper.h"],
    visibility = ["//ecclesia/lib/redfish:__subpackages__"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "id_mapper_devpath",
    srcs = ["id_mapper_devpath.cc"],
    hdrs = ["id_mapper_devpath.h"],
    visibility = ["//ecclesia/lib/redfish:__subpackages__"],
    deps = [
        ":id_mapper",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "id_mapper_devpath_test",
    srcs = ["id_mapper_devpath_test.cc"],
    deps = [
        ":id_mapper_devpath",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "parsers",
    srcs = ["parsers.cc"],
    hdrs = ["parsers.h"],
    visibility = ["//ecclesia/lib/redfish:__subpackages__"],
    deps = [
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_cc_proto",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_protobuf//:protobuf",
    ],
)
