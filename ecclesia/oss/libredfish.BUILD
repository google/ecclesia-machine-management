licenses(["notice"])

exports_files(["LICENSE.md"])

cc_library(
    name = "libredfish",
    srcs = glob(
        ["src/**"],
        exclude = ["src/main.cc"],
    ),
    hdrs = glob(["include/**"]),
    copts = [
        "-DNO_CZMQ",
    ],
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@curl",
        "@jansson",
    ],
)
