"""Macros for producing Redfish accessors."""

load("//ecclesia/build_defs:proto.bzl", "proto_data")

def cc_redfish_accessor_library(name, descriptor_proto_src):
    """Macro for producing a cc_library rule which produces C++ Redfish accessors.

    Produces a cc_library with label matching $name.

    Also produces $name.cc and $name.h as outputs, with their directory path being the same as
    the BUILD file invoking this macro.

    Args:
      name: name of the cc_library rule to be produced.
      # TODO(b/234745697): Instead of a textpb descriptor, provide the Redfish profile.
      descriptor_proto_src: .textpb or .textproto of the descriptor.
    """
    proto_data_name = "{}__accessor_proto_data".format(name)
    proto_data_label = ":" + proto_data_name
    genrule_name = "{}__genrule".format(name)
    cc_out_filename = "{}.cc".format(name)
    h_out_filename = "{}.h".format(name)

    # Turn a textpb descriptor file and compile it into a binarypb.
    # TODO(b/234745697): Instead of a textpb file as input, have a rule which produces binarypb
    # out of a Redfish profile.
    proto_data(
        name = proto_data_name,
        src = descriptor_proto_src,
        out = descriptor_proto_src.replace("textpb", "pb").replace("textproto", "pb"),
        proto_deps = ["//ecclesia/lib/redfish/toolchain:descriptor_proto"],
        proto_name = "ecclesia.Profile",
    )

    # Run the jinja generator tool which takes a descriptor textpb and outputs the C++ files.
    native.genrule(
        name = genrule_name,
        srcs = [proto_data_label],
        outs = [
            cc_out_filename,
            h_out_filename,
        ],
        cmd = "$(location //ecclesia/lib/redfish/toolchain/accessors:generate) --proto_path_in=$(location {}) --cc_path=$(location {}) --h_path=$(location {})".format(proto_data_label, cc_out_filename, h_out_filename),
        exec_tools = ["//ecclesia/lib/redfish/toolchain/accessors:generate"],
    )

    # Compile the generated C++ into a library.
    native.cc_library(
        name = name,
        srcs = [cc_out_filename],
        hdrs = [h_out_filename],
        deps = [
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/time",
            "//ecclesia/lib/redfish:interface",
        ],
    )
