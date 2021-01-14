licenses(["notice"])

exports_files(["LICENSE"])

include_files = [
    "ncurses-6.1/include/ncurses/cursesapp.h",
    "ncurses-6.1/include/ncurses/cursesf.h",
    "ncurses-6.1/include/ncurses/curses.h",
    "ncurses-6.1/include/ncurses/cursesm.h",
    "ncurses-6.1/include/ncurses/cursesp.h",
    "ncurses-6.1/include/ncurses/cursesw.h",
    "ncurses-6.1/include/ncurses/cursslk.h",
    "ncurses-6.1/include/ncurses/eti.h",
    "ncurses-6.1/include/ncurses/etip.h",
    "ncurses-6.1/include/ncurses/form.h",
    "ncurses-6.1/include/ncurses/menu.h",
    "ncurses-6.1/include/ncurses/nc_tparm.h",
    "ncurses-6.1/include/ncurses/ncurses_dll.h",
    "ncurses-6.1/include/ncurses/ncurses.h",
    "ncurses-6.1/include/ncurses/panel.h",
    "ncurses-6.1/include/ncurses/termcap.h",
    "ncurses-6.1/include/ncurses/term_entry.h",
    "ncurses-6.1/include/ncurses/term.h",
    "ncurses-6.1/include/ncurses/tic.h",
    "ncurses-6.1/include/ncurses/unctrl.h",
]

lib_files = [
    "ncurses-6.1/lib/libform.a",
    "ncurses-6.1/lib/libform_g.a",
    "ncurses-6.1/lib/libmenu.a",
    "ncurses-6.1/lib/libmenu_g.a",
    "ncurses-6.1/lib/libncurses++.a",
    "ncurses-6.1/lib/libncurses.a",
    "ncurses-6.1/lib/libncurses++_g.a",
    "ncurses-6.1/lib/libncurses_g.a",
    "ncurses-6.1/lib/libpanel.a",
    "ncurses-6.1/lib/libpanel_g.a",
]

genrule(
    name = "ncurses_srcs",
    srcs = glob(["**"]),
    outs = include_files + lib_files,
    cmd = "\n".join([
        "export INSTALL_DIR=$$(pwd)/$(@D)/ncurses-6.1",
        "export TMP_DIR=$$(mktemp -d -t ncurses.XXXXXX)",
        "mkdir -p $$TMP_DIR",
        "cp -R $$(pwd)/external/ncurses/* $$TMP_DIR",
        "cd $$TMP_DIR",
        "./configure --prefix=$$INSTALL_DIR CFLAGS=-fPIC CXXFLAGS=-fPIC --enable-shared=no",
        "make install",
        "rm -rf $$TMP_DIR",
    ]),
)

cc_library(
    name = "ncurses",
    srcs = [
        "ncurses-6.1/lib/libncurses.a",
        "ncurses-6.1/lib/libncurses++.a",
    ],
    hdrs = include_files,
    includes = ["ncurses-6.1/ncurse/include"],
    linkopts = ["-lpthread"],
    linkstatic = 1,
    visibility = ["//visibility:public"],
)
