load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

package(default_applicable_licenses = [""])

licenses(["notice"])

bzl_library(
    name = "accessors",
    srcs = ["accessors.bzl"],
    visibility = [
        "//visibility:private",  # Only private by automation, not intent. Owner may accept CLs adding visibility. See go/scheuklappen#explicit-private.
    ],
)
