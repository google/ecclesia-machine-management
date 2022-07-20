"""Macros for producing Redfish accessors."""

def _cc_redfish_accessor_library_ipml(ctx):
    outputs = []

    # Generate descriptor proto from the input profile and CSDL
    redfish_profile_descriptor_name = "{}__profile_descriptor.pb".format(ctx.attr.base_name)
    descriptor_proto_out = ctx.actions.declare_file(redfish_profile_descriptor_name)
    outputs.append(descriptor_proto_out)
    if len(ctx.attr.csdl_files) == 0:
        fail("CSDL files cannot be empty")
    profile_to_descriptor_args = ctx.actions.args()
    profile_to_descriptor_args.add("--profile", ctx.file.redfish_profile.path)
    profile_to_descriptor_args.add_all("--csdl_files", ctx.files.csdl_files)
    profile_to_descriptor_args.add("--output_file", descriptor_proto_out.path)

    ctx.actions.run(
        inputs = [ctx.file.redfish_profile] + ctx.files.csdl_files,
        arguments = [profile_to_descriptor_args],
        executable = ctx.executable._profile_to_descriptor_tool,
        outputs = [descriptor_proto_out],
    )

    # Generate .h and .cc files from output descriptor proto
    header_out = ctx.outputs.header_output
    source_out = ctx.outputs.source_output
    outputs.append(header_out)
    outputs.append(source_out)

    descriptor_to_cc_args = ctx.actions.args()
    descriptor_to_cc_args.add("--proto_path_in", descriptor_proto_out.path)
    descriptor_to_cc_args.add("--h_path", header_out.path)
    descriptor_to_cc_args.add("--cc_path", source_out.path)
    descriptor_to_cc_args.add("--h_include", header_out.short_path)

    ctx.actions.run(
        inputs = [descriptor_proto_out],
        arguments = [descriptor_to_cc_args],
        executable = ctx.executable._descriptor_to_cc_tool,
        outputs = [header_out, source_out],
    )

    return DefaultInfo(
        files = depset(outputs),
        runfiles = ctx.runfiles(files = outputs),
    )

_cc_redfish_accessor_library_generate = rule(
    implementation = _cc_redfish_accessor_library_ipml,
    attrs = {
        "base_name": attr.string(),
        "csdl_files": attr.label_list(allow_files = True),
        "redfish_profile": attr.label(allow_single_file = True),
        "header_output": attr.output(),
        "source_output": attr.output(),
        "_profile_to_descriptor_tool": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//ecclesia/lib/redfish/toolchain/internal:profile_to_descriptor_main"),
        ),
        "_descriptor_to_cc_tool": attr.label(
            executable = True,
            cfg = "exec",
            allow_files = True,
            default = Label("//ecclesia/lib/redfish/toolchain/internal/accessors:generate"),
        ),
    },
    output_to_genfiles = True,
)

def cc_redfish_accessor_library(name, csdl_files, redfish_profile, visibility = None):
    """Macro for producing a cc_library rule which produces C++ Redfish accessors.

    Produces a cc_library with label matching $name.

    Also produces $name.cc and $name.h as outputs, with their directory path being the same as
    the BUILD file invoking this macro.

    Args:
      name: name of the cc_library rule to be produced.
      csdl_files: List of all schema files.
      redfish_profile: Filepath to the Redfish Profile to process.
      visibility: visibility of cc_library generated.
    """
    genrule_name = "{}__genrule".format(name)

    cc_out_filename = "{}.cc".format(name)
    h_out_filename = "{}.h".format(name)

    _cc_redfish_accessor_library_generate(
        name = genrule_name,
        base_name = name,
        csdl_files = csdl_files,
        redfish_profile = redfish_profile,
        header_output = h_out_filename,
        source_output = cc_out_filename,
        visibility = ["//visibility:private"],
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
        visibility = visibility,
    )
