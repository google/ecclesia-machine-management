"""Macros for producing Redfish accessors."""

def _redfish_profile_descriptor(name, csdl_files, redfish_profile, output, debug_output = None):
    """Create a Profile descriptor proto from provided Redfish profile and CSDL files.

    Produces a binary proto file at the output. If the debug_output is set, an equivalent textproto
    file will be generated.

    Args:
      name: Name of the genrule to create the protobuffer data.
      csdl_files: List of all schema files.
      redfish_profile: Filepath to the Redfish Profile to process.
      output: Location to output the binary proto data.
      debug_output: If set, location to output the text proto data.
    """
    outs = [output]
    if debug_output:
        outs.append(debug_output)

    if len(csdl_files) == 0:
        fail("CSDL files cannot be empty")

    debug_command = "--debug_output=$(location {})".format(debug_output) if debug_output else ""

    csdl_sources = []
    for csdl in csdl_files:
        csdl_sources.append("$(location {})".format(csdl))

    command = \
        """$(location //ecclesia/lib/redfish/toolchain/internal:profile_to_descriptor_main) \
        --csdl_files {} --profile=$(location {}) --output_file=$(location {}) {}""".format(
            " ".join(csdl_sources),
            redfish_profile,
            output,
            debug_command,
        )

    native.genrule(
        name = name,
        srcs = csdl_files + [redfish_profile],
        outs = outs,
        cmd = command,
        exec_tools = ["//ecclesia/lib/redfish/toolchain/internal:profile_to_descriptor_main"],
    )

def cc_redfish_accessor_library(name, csdl_files, redfish_profile):
    """Macro for producing a cc_library rule which produces C++ Redfish accessors.

    Produces a cc_library with label matching $name.

    Also produces $name.cc and $name.h as outputs, with their directory path being the same as
    the BUILD file invoking this macro.

    Args:
      name: name of the cc_library rule to be produced.
      csdl_files: List of all schema files.
      redfish_profile: Filepath to the Redfish Profile to process.
    """
    redfish_profile_descriptor_name = "{}__profile_descriptor".format(name)
    genrule_name = "{}__genrule".format(name)
    cc_out_filename = "{}.cc".format(name)
    h_out_filename = "{}.h".format(name)
    output_proto_name = "{}__descriptor_proto.pb".format(name)

    # Generate Profile descriptor proto
    # TODO(b/234745697): Add in custom flag generating debug output proto
    _redfish_profile_descriptor(
        name = redfish_profile_descriptor_name,
        csdl_files = csdl_files,
        redfish_profile = redfish_profile,
        output = output_proto_name,
    )

    # Run the jinja generator tool which takes a descriptor textpb and outputs the C++ files.
    native.genrule(
        name = genrule_name,
        srcs = [output_proto_name],
        outs = [
            cc_out_filename,
            h_out_filename,
        ],
        cmd = "$(location //ecclesia/lib/redfish/toolchain/internal/accessors:generate) --proto_path_in=$(location {}) --cc_path=$(location {}) --h_path=$(location {})".format(output_proto_name, cc_out_filename, h_out_filename),
        exec_tools = ["//ecclesia/lib/redfish/toolchain/internal/accessors:generate"],
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
