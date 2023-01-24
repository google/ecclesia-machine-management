"""Build rules to statically build a set of RedPath queries."""

load("//ecclesia/build_defs:proto.bzl", "proto_data")
load("//ecclesia/build_defs:embed.bzl", "cc_data_library")

def redpath_queries(name, srcs, cc_namespace, var_name, visibility = None):
    """Given a set of RedPath queries, compile and create data library to access them.

    Args:
        name: Name of the resulting library.
        srcs: A list of source RedPath textproto queries to compile into the library.
        cc_namespace: C++ namespace for underlying data to be accessible
        var_name: C++ variable name for underlying data to use
        visibility: Built rule visibility for the final data.
    """

    # Convert each file in the srcs into a binary proto
    source_rule_names = []
    for src in srcs:
        if not src.endswith(".textproto") and not src.endswith(".textpb"):
            fail("Textproto files must end in '.textproto' or '.textpb'. Violating src file: %s" % (src))

        # Remove the extension so that we can construct a rule name from it
        find_textproto = src.find(".textproto")
        find_textpb = src.find(".textpb")
        no_suffix_src = src
        if find_textproto > -1:
            no_suffix_src = src[0:find_textproto]
        if find_textpb > -1:
            no_suffix_src = src[0:find_textpb]
        src_rule_name = "%s__%s" % (name, no_suffix_src)
        source_rule_names.append(src_rule_name)
        proto_data(
            name = src_rule_name,
            src = src,
            proto_name = "ecclesia.DelliciusQuery",
            proto_deps = ["//ecclesia/lib/redfish/dellicius/query:query_proto"],
        )

    # Bundle all of the output files into
    cc_data_library(
        name = name,
        data = source_rule_names,
        cc_namespace = cc_namespace,
        var_name = var_name,
        visibility = visibility,
    )
