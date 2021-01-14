# BUILD rules for mockup servers. No other BUILD rules other than
# redfish_mockup should be defined in this file.

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

bzl_library(
    name = "build_defs",
    srcs = ["build_defs.bzl"],
    deps = ["//ecclesia/build_defs:embed"],
)

filegroup(
    name = "serve_mockup",
    srcs = ["serve_mockup.sh"],
    visibility = [":redfish_mockup_rules"],
)

# Package group for all redfish_mockups build rules that require common libs.
package_group(
    name = "redfish_mockup_rules",
    packages = [
        "//ecclesia/redfish_mockups/...",
    ],
)
