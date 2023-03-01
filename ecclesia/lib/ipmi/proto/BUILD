# IPMI-spec derived proto definitions.

package(default_applicable_licenses = [""])

licenses(["notice"])

proto_library(
    name = "entity",
    srcs = ["entity.proto"],
    features = ["disable_pbh"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "entity_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":entity"],
)

proto_library(
    name = "fru",
    srcs = ["fru.proto"],
    features = ["disable_pbh"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "fru_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":fru"],
)
