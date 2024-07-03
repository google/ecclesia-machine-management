"""Load dependencies needed to compile the Ecclesia project.

This function should be called before the ecclesia_deps_second function.

Typical usage in a WORKSPACE file:
load("@com_google_ecclesia//ecclesia/build_defs:deps_first.bzl", "ecclesia_deps_first")
ecclesia_deps_first()

load("@com_google_ecclesia//ecclesia/build_defs:deps_second.bzl", "ecclesia_deps_second")
ecclesia_deps_second()
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _format_oss_path(oss_path, package_name):
    """Formats the oss path with the package name.

    Args:
      oss_path: The path to the build file within the
        //ecclesia/oss directory.
      package_name: The name of the Ecclesia external package.

    Returns:
      The formatted path.
    """

    return "@{}//ecclesia/oss:{}".format(package_name, oss_path)

def _make_patch_paths(patch_dir, patch_files, package_name):
    """Makes a list of patch paths from the directory and filenames.

    Args:
      patch_dir: The directory within //ecclesia/oss containing
        the patches to apply.
      patch_files: A list of strings indicating the
        patches within the patch_dir to use.
      package_name: The name of the Ecclesia external package.

    Returns:
      A formatted list of string paths.
    """

    return [
        _format_oss_path("{}/{}".format(patch_dir, filename), package_name)
        for filename in patch_files
    ]

def ecclesia_deps_first(package_name = "com_google_ecclesia"):
    """Loads common dependencies.

    Normally, users will pull in the Ecclesia package via

    git_repository(
          name = "com_google_ecclesia",
          ...
    )

    If another name is used, it needs to be forwarded here to resolve
    patches that need to be applied.

    Args:
      package_name: The name of the Ecclesia external package.
    """
    if not native.existing_rule("com_google_absl"):
        # Abseil. Picked up Abseil LTS 20230802.2.
        http_archive(
            name = "com_google_absl",
            sha256 = "7c11539617af1f332f0854a6fb21e296a1b29c27d03f23c7b49d4adefcd102cc",
            strip_prefix = "abseil-cpp-20230802.2",
            urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.2.tar.gz"],
        )

    if not native.existing_rule("com_google_googletest"):
        # Google Test. Official release 1.10.0.
        http_archive(
            name = "com_google_googletest",
            sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
            strip_prefix = "googletest-release-1.10.0",
            urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
        )

    if not native.existing_rule("com_github_google_benchmark"):
        # Google benchmark. Official release 1.5.6.
        http_archive(
            name = "com_github_google_benchmark",
            sha256 = "789f85b4810d13ff803834ea75999e41b326405d83d6a538baf01499eda96102",
            strip_prefix = "benchmark-1.5.6",
            urls = ["https://github.com/google/benchmark/archive/refs/tags/v1.5.6.tar.gz"],
        )

    if not native.existing_rule("com_google_absl_py"):
        # Abseil Python. Only release is v1.0.0 from Nov 2021. Pick a commit from Mar 29, 2022.
        http_archive(
            name = "com_google_absl_py",
            sha256 = "5e683d5b63d177977d07f653fb17af3797aacde57de3608ce64a6b152b510fb2",
            strip_prefix = "abseil-py-ce06222fa6feae6f7569310e7af6b9baa97e388a",
            urls = ["https://github.com/abseil/abseil-py/archive/ce06222fa6feae6f7569310e7af6b9baa97e388a.zip"],
        )

    if not native.existing_rule("six_archive"):
        # Six is a dependency of com_google_absl_py. Arbitrarily pick v1.10.0.
        http_archive(
            name = "six_archive",
            urls = [
                "http://mirror.bazel.build/pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz",
                "https://pypi.python.org/packages/source/s/six/six-1.10.0.tar.gz",
            ],
            sha256 = "105f8d68616f8248e24bf0e9372ef04d3cc10104f1980f54d57b2ce73a5ad56a",
            strip_prefix = "six-1.10.0",
            build_file = "@com_google_absl_py//third_party:six.BUILD",
        )

    if not native.existing_rule("com_google_emboss"):
        # Emboss. Uses the latest commit as of May 19, 2021.
        http_archive(
            name = "com_google_emboss",
            sha256 = "53971feb699d35cd96986cf451eb85986974d65429807c3c2b168c6786846b34",
            strip_prefix = "emboss-5cb347f85c9f1d2b7d00c29bd08ef706d8cd0461",
            urls = ["https://github.com/google/emboss/archive/5cb347f85c9f1d2b7d00c29bd08ef706d8cd0461.tar.gz"],
        )

    if not native.existing_rule("com_google_protobuf"):
        # Protocol buffers. Official release 3.17.0.
        http_archive(
            name = "com_google_protobuf",
            sha256 = "eaba1dd133ac5167e8b08bc3268b2d33c6e9f2dcb14ec0f97f3d3eed9b395863",
            strip_prefix = "protobuf-3.17.0",
            urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.17.0.tar.gz"],
        )

    if not native.existing_rule("com_google_googleapis"):
        # Google APIs. Latest commit as of Nov 16, 2020.
        http_archive(
            name = "com_google_googleapis",
            sha256 = "979859a238e6626850fee33d30f4240e90e71009786a55a15134df582dbc2dbe",
            strip_prefix = "googleapis-8d245ac97e058b541f1477b1a85d676b18e80849",
            urls = ["https://github.com/googleapis/googleapis/archive/8d245ac97e058b541f1477b1a85d676b18e80849.tar.gz"],
        )

    if not native.existing_rule("boringssl"):
        # Needs to come before grpc_deps() to be respected.
        patch_files = [
            "01.no_array_parameter.patch",
        ]
        http_archive(
            name = "boringssl",
            patches = _make_patch_paths("boringssl.patches", patch_files, package_name),
            sha256 = "66e1b0675d58b35f9fe3224b26381a6d707c3293eeee359c813b4859a6446714",
            strip_prefix = "boringssl-9b7498d5aba71e545747d65dc65a4d4424477ff0",
            urls = [
                "https://github.com/google/boringssl/archive/9b7498d5aba71e545747d65dc65a4d4424477ff0.tar.gz",
            ],
        )

    if not native.existing_rule("com_github_grpc_grpc"):
        # gRPC. Taken from commit to include compiler fix for gcc error.
        patch_files = [
            # Add visibility to GRPC testing
            "grpc.visibility.patch",
            # Remove any iOS dependencies
            "grpc.delete_ios.patch",
            # Workaround for GCC bug
            "grpc.xds_listener.patch",
        ]
        http_archive(
            name = "com_github_grpc_grpc",
            patches = _make_patch_paths("grpc.patches", patch_files, package_name),
            patch_args = ["-p1"],
            sha256 = "b55696fb249669744de3e71acc54a9382bea0dce7cd5ba379b356b12b82d4229",
            strip_prefix = "grpc-1.51.1",
            urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.51.1.tar.gz"],
        )

    if not native.existing_rule("bazel_skylib"):
        # Skylib libraries.
        http_archive(
            name = "bazel_skylib",
            sha256 = "61352d78e4a89405853b939853cf76d7c323f90e5507f25a22fa523acb71ea14",
            strip_prefix = "bazel-skylib-1.2.0",
            urls = ["https://github.com/bazelbuild/bazel-skylib/archive/refs/tags/1.2.0.tar.gz"],
        )

    if not native.existing_rule("rules_python"):
        # Extra build rules for various languages.
        http_archive(
            name = "rules_python",
            sha256 = "e5470e92a18aa51830db99a4d9c492cc613761d5bdb7131c04bd92b9834380f6",
            strip_prefix = "rules_python-4b84ad270387a7c439ebdccfd530e2339601ef27",
            urls = ["https://github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz"],
        )

    if not native.existing_rule("rules_pkg"):
        http_archive(
            name = "rules_pkg",
            sha256 = "8a298e832762eda1830597d64fe7db58178aa84cd5926d76d5b744d6558941c2",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.7.0/rules_pkg-0.7.0.tar.gz",
                "https://github.com/bazelbuild/rules_pkg/releases/download/0.7.0/rules_pkg-0.7.0.tar.gz",
            ],
        )

    if not native.existing_rule("build_bazel_rules_swift"):
        http_archive(
            # Needed for gRPC.
            name = "build_bazel_rules_swift",
            sha256 = "d0833bc6dad817a367936a5f902a0c11318160b5e80a20ece35fb85a5675c886",
            strip_prefix = "rules_swift-3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8",
            urls = ["https://github.com/bazelbuild/rules_swift/archive/3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8.tar.gz"],
        )

    if not native.existing_rule("com_github_nelhage_rules_boost"):
        # Build rules for boost, which will fetch all of the boost libs. They're then
        # available under @boost//, e.g. @boost//:graph for the graph library.
        http_archive(
            name = "com_github_nelhage_rules_boost",
            sha256 = "0a1d884aa13201b705f93b86d2d0be1de867f6e592de3c4a3bbe6d04bdddf593",
            strip_prefix = "rules_boost-a32cad61d9166d28ed86d0e07c0d9bca8db9cb82",
            urls = ["https://github.com/nelhage/rules_boost/archive/a32cad61d9166d28ed86d0e07c0d9bca8db9cb82.tar.gz"],
        )

    if not native.existing_rule("io_bazel_rules_closure"):
        # TensorFlow depends on "io_bazel_rules_closure" so we need this here.
        # Needs to be kept in sync with the same target in TensorFlow's WORKSPACE file.
        http_archive(
            name = "io_bazel_rules_closure",
            sha256 = "9b99615f73aa574a2947226c6034a6f7c319e1e42905abc4dc30ddbbb16f4a31",
            strip_prefix = "rules_closure-4a79cc6e6eea5e272fe615db7c98beb8cf8e7eb5",
            urls = [
                "http://mirror.tensorflow.org/github.com/bazelbuild/rules_closure/archive/4a79cc6e6eea5e272fe615db7c98beb8cf8e7eb5.tar.gz",
                "https://github.com/bazelbuild/rules_closure/archive/4a79cc6e6eea5e272fe615db7c98beb8cf8e7eb5.tar.gz",  # 2019-09-16
            ],
        )

    if not native.existing_rule("com_github_libevent_libevent"):
        http_archive(
            name = "com_github_libevent_libevent",
            build_file = _format_oss_path("libevent.BUILD", package_name),
            sha256 = "8836ad722ab211de41cb82fe098911986604f6286f67d10dfb2b6787bf418f49",
            strip_prefix = "libevent-release-2.1.12-stable",
            urls = [
                "https://github.com/libevent/libevent/archive/release-2.1.12-stable.zip",
            ],
        )

    if not native.existing_rule("com_googlesource_code_re2"):
        http_archive(
            name = "com_googlesource_code_re2",
            sha256 = "26155e050b10b5969e986dab35654247a3b1b295e0532880b5a9c13c0a700ceb",
            strip_prefix = "re2-2021-06-01",
            urls = [
                "https://github.com/google/re2/archive/refs/tags/2021-06-01.tar.gz",
            ],
        )

    if not native.existing_rule("org_tensorflow"):
        http_archive(
            name = "org_tensorflow",
            # NOTE: when updating this, MAKE SURE to also update the protobuf_js runtime version
            # in third_party/workspace.bzl to >= the protobuf/protoc version provided by TF.
            sha256 = "48ddba718da76df56fd4c48b4bbf4f97f254ba269ec4be67f783684c75563ef8",
            strip_prefix = "tensorflow-2.0.0-rc0",
            urls = [
                "http://mirror.tensorflow.org/github.com/tensorflow/tensorflow/archive/v2.0.0-rc0.tar.gz",  # 2019-08-23
                "https://github.com/tensorflow/tensorflow/archive/v2.0.0-rc0.tar.gz",
            ],
        )

    if not native.existing_rule("com_google_tensorflow_serving"):
        patch_file = "tensorflow.visibility.patch"
        http_archive(
            name = "com_google_tensorflow_serving",
            patches = _make_patch_paths("tensorflow.patches", [patch_file], package_name),
            sha256 = "013fb45df1f8d18c1a912aafa793834adab39a667705e84862880bc72eaac199",
            strip_prefix = "serving-a17d0388ff0146c6ee2b66c75054845cdbd26677",
            urls = ["https://github.com/tensorflow/serving/archive/a17d0388ff0146c6ee2b66c75054845cdbd26677.zip"],
        )

    if not native.existing_rule("com_json"):
        # JSON for Modern C++ version 3.9.1.
        http_archive(
            name = "com_json",
            build_file = _format_oss_path("json.BUILD", package_name),
            sha256 = "4cf0df69731494668bdd6460ed8cb269b68de9c19ad8c27abc24cd72605b2d5b",
            strip_prefix = "json-3.9.1",
            urls = ["https://github.com/nlohmann/json/archive/v3.9.1.tar.gz"],
        )

    if not native.existing_rule("zlib"):
        http_archive(
            name = "zlib",
            build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
            sha256 = "b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30",
            strip_prefix = "zlib-1.2.13",
            urls = ["https://zlib.net/fossils/zlib-1.2.13.tar.gz"],
        )

    if not native.existing_rule("ncurses"):
        http_archive(
            name = "ncurses",
            build_file = _format_oss_path("ncurses.BUILD", package_name),
            sha256 = "30306e0c76e0f9f1f0de987cf1c82a5c21e1ce6568b9227f7da5b71cbea86c9d",
            strip_prefix = "ncurses-6.2",
            urls = [
                "http://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.2.tar.gz",
            ],
        )

    if not native.existing_rule("libedit"):
        http_archive(
            name = "libedit",
            build_file = _format_oss_path("libedit.BUILD", package_name),
            sha256 = "dbb82cb7e116a5f8025d35ef5b4f7d4a3cdd0a3909a146a39112095a2d229071",
            strip_prefix = "libedit-20191231-3.1",
            urls = [
                "https://www.thrysoee.dk/editline/libedit-20191231-3.1.tar.gz",
            ],
        )

    if not native.existing_rule("curl"):
        http_archive(
            name = "curl",
            build_file = _format_oss_path("curl.BUILD", package_name),
            sha256 = "01ae0c123dee45b01bbaef94c0bc00ed2aec89cb2ee0fd598e0d302a6b5e0a98",
            strip_prefix = "curl-7.69.1",
            urls = [
                "https://storage.googleapis.com/mirror.tensorflow.org/curl.haxx.se/download/curl-7.69.1.tar.gz",
                "https://curl.haxx.se/download/curl-7.69.1.tar.gz",
            ],
        )

    if not native.existing_rule("jansson"):
        http_archive(
            name = "jansson",
            build_file = _format_oss_path("jansson.BUILD", package_name),
            sha256 = "5f8dec765048efac5d919aded51b26a32a05397ea207aa769ff6b53c7027d2c9",
            strip_prefix = "jansson-2.12",
            urls = [
                "https://digip.org/jansson/releases/jansson-2.12.tar.gz",
            ],
        )

    if not native.existing_rule("redfishMockupServer"):
        patch_files = [
            "0001-googlify-import-and-add-ipv6-support.patch",
            "0002-logger-level-to-critical.patch",
            "0003-patch-dir-traversal-vulnerability.patch",
            "0004-add-uds-support.patch",
            "0005-support-payload-post.patch",
            "0006-Reply-payload-and-token-headers-if-post-to-Sessions.patch",
            "0007-Add-mTLS-support.patch",
            "0008-Add-EventService-support.patch",
            "0009-add-link-local-support.patch",
            "0010-add-google-service-root-support.patch",
            "0012-Fix-import-path-for-py_binary.patch",
        ]
        http_archive(
            name = "redfishMockupServer",
            build_file = _format_oss_path("redfishMockupServer.BUILD", package_name),
            patches = _make_patch_paths("redfishMockupServer.patches", patch_files, package_name),
            patch_args = ["-p1"],
            sha256 = "63a144428b0bdb0203ed302c6e9d58fba9dac5e1e399bc6a49cbb30c14b05c23",
            strip_prefix = "Redfish-Mockup-Server-04238cb8b7b8d8d8a62cac6623e2c355a6b73eb8",
            urls = ["https://github.com/DMTF/Redfish-Mockup-Server/archive/04238cb8b7b8d8d8a62cac6623e2c355a6b73eb8.tar.gz"],
        )

    if not native.existing_rule("libsodium"):
        patch_file = "libsodium.01.version_h.patch"
        http_archive(
            name = "libsodium",
            build_file = _format_oss_path("libsodium.BUILD", package_name),
            patches = _make_patch_paths("libsodium.patches", [patch_file], package_name),
            sha256 = "d59323c6b712a1519a5daf710b68f5e7fde57040845ffec53850911f10a5d4f4",
            strip_prefix = "libsodium-1.0.18",
            urls = ["https://github.com/jedisct1/libsodium/archive/1.0.18.tar.gz"],
        )

    if not native.existing_rule("zeromq"):
        patch_file = "zmq.01.add_platform_hpp.patch"
        http_archive(
            name = "zeromq",
            build_file = _format_oss_path("zeromq.BUILD", package_name),
            patches = _make_patch_paths("zeromq.patches", [patch_file], package_name),
            sha256 = "27d1e82a099228ee85a7ddb2260f40830212402c605a4a10b5e5498a7e0e9d03",
            strip_prefix = "zeromq-4.2.1",
            urls = ["https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz"],
        )

    if not native.existing_rule("cppzmq"):
        http_archive(
            name = "cppzmq",
            build_file = _format_oss_path("cppzmq.BUILD", package_name),
            sha256 = "9853e0437d834cbed5d3c223bf1d755cadee70e7c964c6e42c4c6783dee5d02c",
            strip_prefix = "cppzmq-4.7.1",
            urls = ["https://github.com/zeromq/cppzmq/archive/v4.7.1.tar.gz"],
        )

    if not native.existing_rule("jinja2"):
        http_archive(
            name = "jinja2",
            build_file = _format_oss_path("jinja2.BUILD", package_name),
            sha256 = "255fdcbc456f20914bb1616dba866b24b047af001238442280ba06649d9a412e",
            # Strip out the root directory and only keep the src files to make python imports work.
            strip_prefix = "jinja-3.0.3/src/jinja2",
            # https://github.com/pallets/jinja2 links to pallets/jinja.
            urls = ["https://github.com/pallets/jinja/archive/refs/tags/3.0.3.tar.gz"],
        )
    if not native.existing_rule("markupsafe"):
        http_archive(
            name = "markupsafe",
            build_file = _format_oss_path("markupsafe.BUILD", package_name),
            sha256 = "0f83b6d1bf6fa65546221d42715034e7e654845583a84906c5936590f9a7ad8f",
            # Strip out the root directory and only keep the src files to make python imports work.
            strip_prefix = "markupsafe-2.1.1/src/markupsafe/",
            urls = ["https://github.com/pallets/markupsafe/archive/refs/tags/2.1.1.tar.gz"],
        )

    if not native.existing_rule("com_google_riegeli"):
        # Riegeli. Uses the latest commit as of Feb 16, 2022.
        http_archive(
            name = "com_google_riegeli",
            sha256 = "a54dafa634db87723db106bc44ef365b1b442d8862aafbeb5f1d2e922049e587",
            strip_prefix = "riegeli-934428f44a6d120cb6c065315c788aa3a1be6b66",
            url = "https://github.com/google/riegeli/archive/934428f44a6d120cb6c065315c788aa3a1be6b66.tar.gz",
        )

    if not native.existing_rule("org_brotli"):
        # Additional projects needed by riegeli.
        patch_files = ["brotli.patch"]
        http_archive(
            name = "org_brotli",
            patch_args = ["-p1"],
            patches = _make_patch_paths("brotli.patches", patch_files, package_name),
            sha256 = "fec5a1d26f3dd102c542548aaa704f655fecec3622a24ec6e97768dcb3c235ff",
            strip_prefix = "brotli-68f1b90ad0d204907beb58304d0bd06391001a4d",
            urls = ["https://github.com/google/brotli/archive/68f1b90ad0d204907beb58304d0bd06391001a4d.zip"],
        )

    if not native.existing_rule("net_zstd"):
        http_archive(
            name = "net_zstd",
            build_file = "@com_google_riegeli//third_party:net_zstd.BUILD",
            sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
            strip_prefix = "zstd-1.4.5/lib",
            urls = ["https://github.com/facebook/zstd/archive/v1.4.5.zip"],
        )

    if not native.existing_rule("highwayhash"):
        http_archive(
            name = "highwayhash",
            build_file = "@com_google_riegeli//third_party:highwayhash.BUILD",
            sha256 = "cf891e024699c82aabce528a024adbe16e529f2b4e57f954455e0bf53efae585",
            strip_prefix = "highwayhash-276dd7b4b6d330e4734b756e97ccfb1b69cc2e12",
            urls = ["https://github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip"],
        )

    if not native.existing_rule("snappy"):
        http_archive(
            name = "snappy",
            build_file = "@com_google_riegeli//third_party:snappy.BUILD",
            sha256 = "e170ce0def2c71d0403f5cda61d6e2743373f9480124bcfcd0fa9b3299d428d9",
            strip_prefix = "snappy-1.1.9",
            urls = ["https://github.com/google/snappy/archive/1.1.9.zip"],
        )

    if not native.existing_rule("public_redfish_schema"):
        http_archive(
            name = "public_redfish_schema",
            build_file = _format_oss_path("public_redfish_schema.BUILD", package_name),
            strip_prefix = "csdl",
            sha256 = "88534f1ec24943fea67ec7b329ebfee8d513de4fd48dfd1d743ce218e76090fe",
            urls = ["https://www.dmtf.org/sites/default/files/standards/documents/DSP8010_2021.4.zip"],
        )
