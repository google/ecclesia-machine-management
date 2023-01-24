load("//ecclesia/lib/redfish/dellicius/engine:redpath_query.bzl", "redpath_queries")

licenses(["notice"])

filegroup(
    name = "sample_queries_in",
    srcs = [
        "query_in/assembly_in.textproto",
        "query_in/malformed_query.textproto",
        "query_in/malformed_query_links.textproto",
        "query_in/processors_in.textproto",
        "query_in/sensor_in.textproto",
        "query_in/sensor_in_links.textproto",
        "query_in/sensor_in_predicates.textproto",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "sample_queries_out",
    srcs = [
        "query_out/assembly_out.textproto",
        "query_out/devpath_assembly_out.textproto",
        "query_out/devpath_sensor_out.textproto",
        "query_out/processors_out.textproto",
        "query_out/sensor_out.textproto",
        "query_out/sensor_out_links.textproto",
        "query_out/sensor_out_predicates.textproto",
    ],
    visibility = ["//visibility:public"],
)

redpath_queries(
    name = "sample_queries_in_rule",
    srcs = glob(["query_in/**"]),
    cc_namespace = "ecclesia",
    var_name = "kSampleQueriesIn",
)
