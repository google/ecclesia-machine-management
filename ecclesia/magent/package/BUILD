load("@rules_pkg//:rpm.bzl", "pkg_rpm")

pkg_rpm(
    name = "indus",
    architecture = "x86_64",
    data = [
        "magent_indus.service",
        "//ecclesia/magent/daemons/indus:magent_indus_oss.stripped",
        "//ecclesia/magent/redfish/assemblies/indus:assemblies_tar",
    ],
    release = "rhel",
    spec_file = "indus.spec",
    # TODO(b/161989677): Add version info, possibly via "version_file" instead
    # of "version" field below.
    version = "0.0.0",
)
