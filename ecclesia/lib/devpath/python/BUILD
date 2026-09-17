load("//devtools/clif/python:clif_build_rule.bzl", "py_clif_cc")

py_clif_cc(
    name = "devpath",
    srcs = ["devpath.clif"],
    visibility = ["//visibility:public"],
    deps = [
        "//ecclesia/lib/devpath",
        "@com_google_absl//absl/python:optional",
    ],
)

py_test(
    name = "devpath_test",
    srcs = ["devpath_test.py"],
    python_version = "PY3",
    deps = [
        ":devpath",
        "//testing/pybase",
    ],
)
