licenses(["notice"])

cc_library(
    name = "indus_platform_translator",
    hdrs = [
        "platform_translator.h",
    ],
    visibility = [
        "//ecclesia:library_users",
    ],
    deps = [
        "//ecclesia/lib/smbios:platform_translator",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_googlesource_code_re2//:re2",
    ],
)
