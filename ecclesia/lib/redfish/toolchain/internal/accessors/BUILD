licenses(["notice"])

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
        "//ecclesia/lib/redfish/toolchain/internal:descriptor_py_pb2",
        "@jinja2",
    ],
)
