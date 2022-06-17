licenses(["notice"])

py_binary(
    name = "generate",
    srcs = ["generate.py"],
    data = [
        "accessors.cc.jinja2",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        "//ecclesia/lib/jinja2:loader",
        "@jinja2",
    ],
)

genrule(
    name = "generate_accessors_cc",
    outs = ["accessors.cc"],
    cmd = "$(location :generate) > $@",
    exec_tools = [":generate"],
)
