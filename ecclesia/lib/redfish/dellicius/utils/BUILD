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
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_test(
    name = "path_util_test",
    srcs = ["path_util_test.cc"],
    deps = [
        ":path_util",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_library(
    name = "id_assigner",
    hdrs = ["id_assigner.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "id_assigner_devpath",
    srcs = ["id_assigner_devpath.cc"],
    hdrs = ["id_assigner_devpath.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":id_assigner",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "id_assigner_devpath_test",
    srcs = ["id_assigner_devpath_test.cc"],
    deps = [
        ":id_assigner_devpath",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "query_validator_test",
    srcs = ["query_validator_test.cc"],
    data = [
        "query_validator_test_invalid_query.textproto",
        "query_validator_test_valid_query.textproto",
    ],
    deps = [
        ":query_validator",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/testing:status",
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
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "parsers_test",
    srcs = ["parsers_test.cc"],
    deps = [
        ":parsers",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_cc_proto",
        "//ecclesia/lib/redfish/dellicius/engine/internal:interface",
        "//ecclesia/lib/redfish/redpath/definitions/query_result:query_result_cc_proto",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "for_each",
    srcs = ["for_each.cc"],
    hdrs = ["for_each.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "@com_google_absl//absl/functional:any_invocable",
    ],
)

cc_test(
    name = "for_each_test",
    srcs = ["for_each_test.cc"],
    deps = [
        ":for_each",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "query_validator",
    srcs = ["query_validator.cc"],
    hdrs = ["query_validator.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "join",
    srcs = ["join.cc"],
    hdrs = ["join.h"],
    visibility = ["//ecclesia/lib/redfish/dellicius:__subpackages__"],
    deps = [
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "join_test",
    srcs = ["join_test.cc"],
    deps = [
        ":join",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)
