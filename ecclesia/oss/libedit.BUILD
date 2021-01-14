licenses(["notice"])

exports_files(["LICENSE"])

include_files = [
    "libedit-3.1/include/histedit.h",
    "libedit-3.1/include/editline/readline.h",
]

lib_files = [
    "libedit-3.1/lib/libedit.a",
]

genrule(
    name = "libedit_srcs",
    srcs = glob(["**"]),
    outs = include_files + lib_files,
    cmd = "\n".join([
        "export INSTALL_DIR=$$(pwd)/$(@D)/libedit-3.1",
        "export TMP_DIR=$$(mktemp -d -t libedit.XXXXXX)",
        "mkdir -p $$TMP_DIR",
        "cp -R $$(pwd)/external/libedit/* $$TMP_DIR",
        "cd $$TMP_DIR",
        "./configure --prefix=$$INSTALL_DIR CFLAGS=-fPIC CXXFLAGS=-fPIC --enable-shared=no",
        "make install",
        "rm -rf $$TMP_DIR",
    ]),
)

cc_library(
    name = "pretend_to_be_gnu_readline_system",
    srcs = [
        "libedit-3.1/lib/libedit.a",
    ],
    hdrs = include_files,
    includes = ["libedit-3.1/libedit/include"],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)
