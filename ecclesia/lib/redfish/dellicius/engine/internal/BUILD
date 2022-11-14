licenses(["notice"])

cc_library(
    name = "interface",
    hdrs = ["interface.h"],
    visibility = ["//ecclesia/lib/redfish/dellicius:__subpackages__"],
    deps = [
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/time:clock",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "normalizer",
    srcs = ["normalizer.cc"],
    hdrs = ["normalizer.h"],
    visibility = [
        "//ecclesia/lib/redfish/dellicius/engine/internal:__subpackages__",
        "//platforms/ecclesia/simplicius_tool:__subpackages__",
    ],
    deps = [
        ":interface",
        "//ecclesia/lib/redfish:devpath",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish:topology",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "query_planner",
    srcs = ["query_planner.cc"],
    hdrs = ["query_planner.h"],
    visibility = [
        "//ecclesia/lib/redfish/dellicius/engine/internal:__subpackages__",
        "//platforms/ecclesia/simplicius_tool:__subpackages__",
    ],
    deps = [
        ":interface",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
        "//ecclesia/lib/redfish/dellicius/query:query_result_cc_proto",
        "//ecclesia/lib/time:clock",
        "//ecclesia/lib/time:proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_library(
    name = "factory",
    hdrs = ["factory.h"],
    visibility = ["//ecclesia/lib/redfish/dellicius:__subpackages__"],
    deps = [
        ":interface",
        "//ecclesia/lib/redfish:interface",
        "//ecclesia/lib/redfish/dellicius/engine/internal:normalizer",
        "//ecclesia/lib/redfish/dellicius/engine/internal:query_planner",
        "//ecclesia/lib/redfish/dellicius/query:query_cc_proto",
    ],
)
