# Presubmit check for validating modified and new redpath query files.
cc_test(
    name = "query_validation_presubmit",
    srcs = ["query_validation_presubmit_main.cc"],
    tags = [
        "manual",
        "notap",
    ],
    deps = [
        "//base",
        "//base:logging",
        "//devtools/api:finding_cc_proto",
        "//devtools/api/source:changelist_presubmit_cc_proto",
        "//devtools/api/source:presubmit_stubby_service_cc_proto",
        "//devtools/api/source:source_file_presubmit_cc_proto",
        "//devtools/api/source/piper:piper_cc_proto",
        "//devtools/api/source/presubmit_build_target:presubmit_utils",
        "//devtools/staticanalysis/findings/api:api_cc_proto",
        "//ecclesia/lib/redfish/dellicius/utils:query_validator",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
