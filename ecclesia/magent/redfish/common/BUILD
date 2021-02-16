licenses(["notice"])

cc_library(
    name = "common",
    hdrs = [
        "memory_collection.h",
        "processor_collection.h",
        "session_collection.h",
        "storage_collection.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/magent/redfish/core:redfish_core",
        "//ecclesia/magent/sysmodel/x86:x86_sysmodel",
        "@com_google_absl//absl/strings",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
        "@com_googlesource_code_re2//:re2",
        "@com_jsoncpp//:json",
    ],
)

cc_test(
    name = "session_collection_test",
    srcs = ["session_collection_test.cc"],
    data = [
        ":test_data/session_collection.json",
    ],
    deps = [
        ":common",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/network:testing",
        "//ecclesia/lib/redfish:raw",
        "//ecclesia/magent/lib/thread_pool",
        "//ecclesia/magent/redfish/core:redfish_core",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/client/public:http_client",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/client/public:http_client_api",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
        "@com_googlesource_code_re2//:re2",
        "@com_jsoncpp//:json",
    ],
)
