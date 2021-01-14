licenses(["notice"])

proto_library(
    name = "magent_config_proto",
    srcs = ["config.proto"],
    visibility = ["//ecclesia:magent_library_users"],
)

cc_proto_library(
    name = "magent_config_cc_proto",
    visibility = ["//ecclesia:magent_library_users"],
    deps = [":magent_config_proto"],
)
