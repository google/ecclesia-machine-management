load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

bzl_library(
    name = "accessors",
    srcs = ["accessors.bzl"],
    deps = ["//ecclesia/build_defs:proto"],
)

py_binary(
    name = "generate",
    srcs = ["generate.py"],
    data = [
        "accessors.cc.jinja2",
        "accessors.h.jinja2",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/jinja2:loader",
        "//ecclesia/lib/redfish/toolchain:descriptor_py_pb2",
        "@jinja2",
    ],
)
