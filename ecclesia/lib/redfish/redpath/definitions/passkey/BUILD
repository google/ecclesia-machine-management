# load("@rules_cc//cc:cc_library.bzl", "cc_library")

licenses(["notice"])

cc_library(
    name = "annotation_passkey",
    hdrs = ["annotation_passkey.h"],
    # We tightly control this visibility for the passkey factory. New clients using the url annotations need to add themselves to the subquery_url_annotation package group.
    visibility = ["//ecclesia:subquery_url_annotation"],
)

cc_library(
    name = "metrics_passkey",
    hdrs = ["metrics_passkey.h"],
    # We tightly control this visibility for the passkey factory. New clients using the query metrics need to add themselves to the query_metrics package group.
    visibility = ["//ecclesia:query_metrics"],
)

cc_library(
    name = "log_redfish_traces_passkey",
    hdrs = ["log_redfish_traces_passkey.h"],
    # We tightly control this visibility for the passkey factory. New clients using the query metrics need to add themselves to the query_metrics package group.
    visibility = ["//ecclesia:log_redfish_traces"],
)
