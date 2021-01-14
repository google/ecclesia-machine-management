licenses(["notice"])

exports_files(["LICENSE"])

include_files = [
    "libjansson/src/jansson.h",
    "libjansson/src/jansson_config.h",
]

lib_files = [
    "libjansson/lib/libjansson.a",
]

genrule(
    name = "jansson_srcs",
    srcs = glob(["**"]),
    outs = include_files + lib_files,
    cmd = "\n".join([
        "export INSTALL_DIR=$$(pwd)/$(@D)/libjansson",
        "export TMP_DIR=$$(mktemp -d -t libjansson.XXXXXX)",
        "mkdir -p $$TMP_DIR",
        "cp -R $$(pwd)/external/jansson/* $$TMP_DIR",
        "cd $$TMP_DIR",
        "./configure --prefix=$$INSTALL_DIR CFLAGS=-fPIC CXXFLAGS=-fPIC --enable-shared=no",
        "make install",
        "rm -rf $$TMP_DIR",
        "cp -R $$INSTALL_DIR/include/* $$INSTALL_DIR/src/",
    ]),
)

cc_library(
    name = "jansson",
    srcs = ["libjansson/lib/libjansson.a"],
    hdrs = include_files,
    includes = ["libjansson/src"],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)
