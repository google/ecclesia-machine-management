load("//tools/build_defs/js/jspb_proto_library:rules.bzl", "jspb_proto_library")

proto_library(
    name = "node_class_assignment_proto",
    srcs = ["node_class_assignment.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "node_class_assignment_cc_proto",
    visibility = [
        "//visibility:public",
    ],
    deps = [":node_class_assignment_proto"],
)

jspb_proto_library(
    name = "node_class_assignment_jspb_proto",
    visibility = [
        "//visibility:public",
    ],
    deps = [":node_class_assignment_proto"],
)
