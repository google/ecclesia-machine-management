"""Starlark definitions for build rules to write files.

For simple cases where you just want to write fixed string content into a file
you can use write_file. Typical usage:

load("//ecclesia/build_defs:file.bzl", "write_file")
write_file(
    name = "my_generated_file",
    content = "my generated content!",
    out = "my_generated_file.txt",
)

Args:
  name: The name of the rule.
  content: The string content to write into the file.
  out: The name of the file to write the content into.
  is_executable: An optional boolean indicating if the file is exectuable.
      Default to false.


For more complex cases where the content of the file is mostly fixed but needs
to be parameterized with some simple substitutions (e.g. inserting names or
paths into a script) you can use expand_template. Typical usage:

load("//ecclesia/build_defs:file.bzl", "expand_template")
expand_template(
    name = "my_generated_file",
    template = "my.template",
    out = "my_generated_file.sh",
    substitutions = {
        "$FOO": "foo_value",
        "$BAR": "bar_value",
    },
    is_executable = True,
)

Args:
  name: The name of the rule.
  template: The file to expand using subsitutions.
  out: The name of the file to write the expanded content into.
  substitutions: A string->string dictionary of substitutions to make.
  is_executable: An optional boolean indicating if the file is exectuable.
      Default to false.
"""

def _write_file_impl(ctx):
    ctx.actions.write(
        output = ctx.outputs.out,
        content = ctx.attr.content,
        is_executable = ctx.attr.is_executable,
    )

write_file = rule(
    implementation = _write_file_impl,
    attrs = {
        "content": attr.string(),
        "out": attr.output(mandatory = True),
        "is_executable": attr.bool(default = False, mandatory = False),
    },
)

def _expand_template_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.out,
        substitutions = ctx.attr.substitutions,
        is_executable = ctx.attr.is_executable,
    )

expand_template = rule(
    implementation = _expand_template_impl,
    attrs = {
        "template": attr.label(mandatory = True, allow_single_file = True),
        "substitutions": attr.string_dict(mandatory = True),
        "out": attr.output(mandatory = True),
        "is_executable": attr.bool(default = False, mandatory = False),
    },
)
