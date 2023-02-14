package(default_applicable_licenses = ["//ecclesia:license"])

licenses(["notice"])

py_library(
    name = "loader",
    srcs = ["loader.py"],
    srcs_version = "PY2AND3",
    # Loader should only be used by the toolchain accessor, lest the loader be updated to work
    # with arbitrary directories.
    visibility = ["//ecclesia:ecclesia_jinja_users"],
    deps = [
        "@jinja2",
    ],
)
