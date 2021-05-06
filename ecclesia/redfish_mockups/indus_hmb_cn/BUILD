# BUILD rules for mockup servers. No BUILD rules other than
# redfish_mockup should be defined in this file.

load("//ecclesia/redfish_mockups:build_defs.bzl", "redfish_mockup")

licenses(["notice"])

redfish_mockup(
    name = "mockup.shar",
    visibility = [
        "//ecclesia:mmanager_binary_tests",
        "//ecclesia:mmanager_middle_users",
        "//ecclesia:redfish_users",
    ],
)
