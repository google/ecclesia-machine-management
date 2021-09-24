licenses(["notice"])

cc_library(
    name = "redfish_core",
    srcs = [
        "assembly.cc",
        "assembly_modifiers.cc",
        "uri_generation.cc",
    ],
    hdrs = [
        "assembly.h",
        "assembly_modifiers.h",
        "index_resource.h",
        "json_helper.h",
        "odata_metadata.h",
        "redfish_keywords.h",
        "resource.h",
        "service_root_resource.h",
        "uri_generation.h",
    ],
    visibility = ["//ecclesia:magent_library_users"],
    deps = [
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:dir",
        "//ecclesia/lib/file:path",
        "//ecclesia/lib/io/pci:location",
        "//ecclesia/lib/logging",
        "//ecclesia/lib/strings:regex",
        "//ecclesia/lib/types:overloaded",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
        "@com_googlesource_code_re2//:re2",
        "@com_json//:json",
    ],
)

cc_test(
    name = "assembly_modifiers_test",
    srcs = ["assembly_modifiers_test.cc"],
    deps = [
        ":redfish_core",
        "//ecclesia/lib/io/pci:location",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_test(
    name = "json_helper_test",
    srcs = ["json_helper_test.cc"],
    deps = [
        ":redfish_core",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_json//:json",
    ],
)

cc_test(
    name = "odata_metadata_test",
    srcs = ["odata_metadata_test.cc"],
    data = [
        "//ecclesia/magent/redfish/metadata:index.xml",
    ],
    deps = [
        ":redfish_core",
        "//ecclesia/lib/apifs",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/network:testing",
        "//ecclesia/magent/lib/thread_pool",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/client/public:http_client",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/client/public:http_client_api",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server",
        "@com_google_tensorflow_serving//tensorflow_serving/util/net_http/server/public:http_server_api",
    ],
)

cc_test(
    name = "uri_generation_test",
    srcs = ["uri_generation_test.cc"],
    deps = [
        ":redfish_core",
        "//ecclesia/lib/io/pci:location",
        "@com_google_googletest//:gtest_main",
    ],
)
