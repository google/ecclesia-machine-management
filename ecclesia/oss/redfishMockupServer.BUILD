licenses(["notice"])

exports_files(["LICENSE"])

py_binary(
    name = "redfishMockupServer",
    srcs = [
        "redfishMockupServer.py",
        "rfSsdpServer.py",
    ],
    python_version = "PY3",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "redfishMockupServer_zip",
    srcs = [":redfishMockupServer"],
    output_group = "python_zip_file",
    visibility = ["//visibility:public"],
)

genrule(
    name = "redfishMockupServer.par",
    srcs = [":redfishMockupServer_zip"],
    outs = ["redfishMockupServer.par"],
    cmd = "echo '#!/usr/bin/env python3' | cat - $(locations :redfishMockupServer_zip) > $@",
    executable = True,
    visibility = ["//visibility:public"],
)
