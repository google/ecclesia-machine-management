licenses(["notice"])

cc_library(
    name = "normalizer",
    srcs = ["normalizer.cc"],
    hdrs = ["normalizer.h"],
    visibility = ["//ecclesia/lib/redfish/dellicius/engine/internal:__subpackages__"],
    deps = [
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/time:proto",
    ],
)

cc_library(
    name = "query_planner",
    srcs = ["query_planner.cc"],
    hdrs = ["query_planner.h"],
    visibility = ["//ecclesia/lib/redfish/dellicius/engine/internal:__subpackages__"],
    deps = [
        "//ecclesia/lib/logging",
        "//ecclesia/lib/protobuf:parse",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/time:clock",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)