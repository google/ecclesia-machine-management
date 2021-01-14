# Set of rules to package the OData Metadata file

load("@rules_pkg//:pkg.bzl", "pkg_tar")

licenses(["notice"])

pkg_tar(
    name = "odata_metadata_pkg_tar",
    srcs = [":index.xml"],
    extension = "tar.gz",
    strip_prefix = ".",
    visibility = [
        "//ecclesia:magent_library_users",
    ],
)

exports_files(
    ["index.xml"],
    visibility = ["//ecclesia:magent_library_users"],
)
