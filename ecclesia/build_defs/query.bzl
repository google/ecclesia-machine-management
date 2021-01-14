"""Starlark definitions for build rules for querying info about other rules.

This provides a rule that can take another build rule and write the output files
of that rule out to a file. This is primarily useful for cases where you have an
build rule that has a data dependency that takes a dynamic set of files (e.g. a
test that has a bunch of data files coming from a glob). Typical usage:

load("//ecclesia/build_defs:query.bzl", "rule_output_files")
rule_output_files(
    name = "output_files",
    rule = ":some_build_rule",
)

Args:
  name: The name of the rule, as well as the name of the output file.
  rule: The rule whose outputs should be captured. The output files will be
      written out one per line.
"""

def _rule_output_files_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(
        output = out,
        content = "\n".join([f.short_path for f in ctx.files.rule]),
    )
    return DefaultInfo(
        files = depset([out]),
        runfiles = ctx.runfiles(files = [out]),
    )

rule_output_files = rule(
    implementation = _rule_output_files_impl,
    attrs = {
        "rule": attr.label(),
    },
)
