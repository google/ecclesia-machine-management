licenses(["notice"])

cc_library(
    name = "redfish_proxy",
    srcs = ["redfish_proxy.cc"],
    hdrs = ["redfish_proxy.h"],
    visibility = ["//ecclesia:mmanager_frontend_users"],
    deps = [
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_grpc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_cc_proto",
        "//ecclesia/lib/redfish/proto:redfish_v1_grpc_include",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
