# BUILD rules for mockup servers. No other BUILD rules other than
# redfish_mockup should be defined in this file.

load("//ecclesia/redfish_mockups:build_defs.bzl", "redfish_mockup")

package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

redfish_mockup(
    name = "mockup.shar",
    visibility = ["//visibility:public"],
)
