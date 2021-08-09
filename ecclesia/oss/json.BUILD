licenses(["unencumbered"])

exports_files(["LICENSE"])

cc_library(
    name = "json",
    hdrs = [
        # For use in third_party only.
        "single_include/nlohmann/json.hpp",
    ],
    copts = [
        "-Wno-google3-literal-operator",
    ],
    includes = ["include"],
    visibility = ["//visibility:public"],
)
