# Set of rules to package the assemblies for individual configurations

load("@rules_pkg//:pkg.bzl", "pkg_tar")

licenses(["notice"])

pkg_tar(
    name = "assemblies_tar",
    srcs = glob(["assemblies/*.json"]),
    extension = "tar.gz",
    strip_prefix = ".",
    visibility = [
        "//ecclesia:magent_library_users",
    ],
)

filegroup(
    name = "assemblies_json_files",
    srcs = glob(["assemblies/*.json"]),
)
