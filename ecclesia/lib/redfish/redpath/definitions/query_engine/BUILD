proto_library(
    name = "query_engine_features_proto",
    srcs = ["query_engine_features.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "query_engine_features_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":query_engine_features_proto"],
)

cc_library(
    name = "query_engine_features",
    hdrs = ["query_engine_features.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":query_engine_features_cc_proto",
        "//ecclesia/lib/redfish/redpath/definitions/passkey:annotation_passkey",
    ],
)

cc_library(
    name = "query_spec",
    srcs = ["query_spec.cc"],
    hdrs = ["query_spec.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/redfish/dellicius/engine:query_rules_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/status:macros",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "redpath_subscription",
    srcs = ["redpath_subscription.cc"],
    hdrs = ["redpath_subscription.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/status:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_json//:json",
    ],
)

cc_test(
    name = "redpath_subscription_test",
    srcs = ["redpath_subscription_test.cc"],
    deps = [
        ":redpath_subscription",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "//ecclesia/lib/redfish/transport:interface",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_test(
    name = "query_spec_test",
    srcs = ["query_spec_test.cc"],
    deps = [
        ":query_spec",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:cc_embed_interface",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/status:test_macros",
        "//ecclesia/lib/testing:proto",
        "//ecclesia/lib/testing:status",
        "//ecclesia/lib/time:clock_fake",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
