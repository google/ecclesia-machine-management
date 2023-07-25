load("//ecclesia/lib/redfish/dellicius/engine:redpath_query.bzl", "redpath_queries")
load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

licenses(["notice"])

filegroup(
    name = "sample_queries_in",
    srcs = [
        "query_in/assembly_in.textproto",
        "query_in/managers_in.textproto",
        "query_in/processors_in.textproto",
        "query_in/sensor_in.textproto",
        "query_in/sensor_in_links.textproto",
        "query_in/sensor_in_predicates.textproto",
        "query_in/service_root_google_in.textproto",
        "query_in/service_root_in.textproto",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "sample_malformed_queries_in",
    srcs = [
        "query_in/malformed_query.textproto",
        "query_in/malformed_query_links.textproto",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "sample_queries_out",
    srcs = [
        "query_out/assembly_out.textproto",
        "query_out/devpath_assembly_out.textproto",
        "query_out/devpath_sensor_out.textproto",
        "query_out/managers_out.textproto",
        "query_out/processors_out.textproto",
        "query_out/sensor_out.textproto",
        "query_out/sensor_out_links.textproto",
        "query_out/sensor_out_predicates.textproto",
        "query_out/service_root_google_out.textproto",
        "query_out/service_root_out.textproto",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "devpath3_queries_in",
    srcs = glob(["devpath3/**"]),
    visibility = ["//visibility:public"],
)

cc_data_library(
    name = "devpath3_queries_embedded",
    cc_namespace = "ecclesia",
    data = [
        ":devpath3_queries_in",
    ],
    var_name = "kDevpath3Queries",
    visibility = ["//visibility:public"],
)

redpath_queries(
    name = "sample_queries_in_rule",
    srcs = glob(["query_in/**"]),
    cc_namespace = "ecclesia",
    var_name = "kSampleQueriesIn",
)
