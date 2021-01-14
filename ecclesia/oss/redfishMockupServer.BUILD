load("@subpar//:subpar.bzl", "par_binary")

licenses(["notice"])

exports_files(["LICENSE"])

par_binary(
    name = "redfishMockupServer",
    srcs = [
        "redfishMockupServer.py",
        "rfSsdpServer.py",
    ],
    python_version = "PY3",
    visibility = ["//visibility:public"],
)
