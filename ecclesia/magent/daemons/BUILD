licenses(["notice"])

cc_library(
    name = "magent_hdr",
    hdrs = ["common.h"],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/magent/lib/thread_pool",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
    ],
)
