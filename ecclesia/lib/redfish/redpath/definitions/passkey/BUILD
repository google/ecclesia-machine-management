licenses(["notice"])

cc_library(
    name = "annotation_passkey",
    hdrs = ["annotation_passkey.h"],
    # We tightly control this visibility for the passkey factory. New clients using the raw RedfishInterface pointer need to add themselves to the redfish_query_engine_raw_intf_users package group.
    visibility = ["//ecclesia:subquery_url_annotation"],
)
