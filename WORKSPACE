workspace(name = "com_google_ecclesia")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Google Test. Official release 1.10.0.
http_archive(
    name = "com_google_googletest",
    sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    strip_prefix = "googletest-release-1.10.0",
    urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
)

# Abseil. Latest feature not releases yet. Picked up a commit from Sep 2, 2020
http_archive(
    name = "com_google_absl",
    sha256 = "5ec35586b685eea11f198bb6e75f870e37fde62d15b95a3897c37b2d0bbd9017",
    strip_prefix = "abseil-cpp-143a27800eb35f4568b9be51647726281916aac9",
    urls = ["https://github.com/abseil/abseil-cpp/archive/143a27800eb35f4568b9be51647726281916aac9.zip"],
)

# Emboss. Uses the latest commit as of Oct 19, 2020.
http_archive(
    name = "com_google_emboss",
    sha256 = "292e5bb5e4bb8a5b75aeeec4def6502dcede288f2017feb2c35834dec775f891",
    strip_prefix = "emboss-2ad6ec5a6501e42a1df23ab35ebd51186851c02c",
    urls = ["https://github.com/google/emboss/archive/2ad6ec5a6501e42a1df23ab35ebd51186851c02c.zip"],
)

# Protocol buffers. Official release 3.14.0.
http_archive(
    name = "com_google_protobuf",
    sha256 = "d0f5f605d0d656007ce6c8b5a82df3037e1d8fe8b121ed42e536f569dec16113",
    strip_prefix = "protobuf-3.14.0",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.14.0.tar.gz"],
    patches = [
        # Force the enabling of optional proto3 fields. In theory you can do
        # this by passing --experimantal_allow_proto3_optional as a flag but not
        # every library (i.e. gRPC) that uses protoc provides a mechanism to
        # pass flags down to it.
        "//ecclesia/oss:com_google_protobuf.patches/default_proto3_optional.patch",
    ],
)

# Google APIs. Latest commit as of Nov 16, 2020.
http_archive(
    name = "com_google_googleapis",
    sha256 = "979859a238e6626850fee33d30f4240e90e71009786a55a15134df582dbc2dbe",
    strip_prefix = "googleapis-8d245ac97e058b541f1477b1a85d676b18e80849",
    urls = ["https://github.com/googleapis/googleapis/archive/8d245ac97e058b541f1477b1a85d676b18e80849.tar.gz"],
)

# Google APIs imports. Required to build googleapis.
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    grpc = True,
)

# gRPC. Official release 1.33.2. Name is required by Google APIs.
http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "2060769f2d4b0d3535ba594b2ab614d7f68a492f786ab94b4318788d45e3278a",
    strip_prefix = "grpc-1.33.2",
    urls = ["https://github.com/grpc/grpc/archive/v1.33.2.tar.gz"],
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

# Skylib libraries.
http_archive(
    name = "bazel_skylib",
    sha256 = "ea5dc9a1d51b861d27608ad3bd6c177bc88d54e946cb883e9163e53c607a9b4c",
    strip_prefix = "bazel-skylib-2b38b2f8bd4b8603d610cfc651fcbb299498147f",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/2b38b2f8bd4b8603d610cfc651fcbb299498147f.tar.gz"],
)

# Extra build rules for various languages.
http_archive(
    name = "rules_python",
    sha256 = "e5470e92a18aa51830db99a4d9c492cc613761d5bdb7131c04bd92b9834380f6",
    strip_prefix = "rules_python-4b84ad270387a7c439ebdccfd530e2339601ef27",
    urls = ["https://github.com/bazelbuild/rules_python/archive/4b84ad270387a7c439ebdccfd530e2339601ef27.tar.gz"],
)
http_archive(
    name = "rules_pkg",
    sha256 = "b9d1387deed06eef45edd3eb7fd166577b8ad1884cb6a17898d136059d03933c",
    strip_prefix = "rules_pkg-0.2.6-1/pkg",
    urls = ["https://github.com/bazelbuild/rules_pkg/archive/0.2.6-1.tar.gz"],
)
http_archive(
    # Needed for gRPC.
    name = "build_bazel_rules_swift",
    sha256 = "d0833bc6dad817a367936a5f902a0c11318160b5e80a20ece35fb85a5675c886",
    strip_prefix = "rules_swift-3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8",
    urls = ["https://github.com/bazelbuild/rules_swift/archive/3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8.tar.gz"],
)

# Build rules for boost, which will fetch all of the boost libs. They're then
# available under @boost//, e.g. @boost//:graph for the graph library.
http_archive(
    name = "com_github_nelhage_rules_boost",
    sha256 = "0a1d884aa13201b705f93b86d2d0be1de867f6e592de3c4a3bbe6d04bdddf593",
    strip_prefix = "rules_boost-a32cad61d9166d28ed86d0e07c0d9bca8db9cb82",
    urls = ["https://github.com/nelhage/rules_boost/archive/a32cad61d9166d28ed86d0e07c0d9bca8db9cb82.tar.gz"],
)
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

# Subpar, an external equivalent for building .par files.
http_archive(
    name = "subpar",
    sha256 = "b80297a1b8d38027a86836dbadc22f55dc3ecad56728175381aa6330705ac10f",
    strip_prefix = "subpar-2.0.0",
    urls = ["https://github.com/google/subpar/archive/2.0.0.tar.gz"],
)

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

http_archive(
    name = "com_github_libevent_libevent",
    build_file = "@//ecclesia/oss:libevent.BUILD",
    sha256 = "8836ad722ab211de41cb82fe098911986604f6286f67d10dfb2b6787bf418f49",
    strip_prefix = "libevent-release-2.1.12-stable",
    urls = [
        "https://github.com/libevent/libevent/archive/release-2.1.12-stable.zip",
    ],
)

http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "d070e2ffc5476c496a6a872a6f246bfddce8e7797d6ba605a7c8d72866743bf9",
    strip_prefix = "re2-506cfa4bffd060c06ec338ce50ea3468daa6c814",
    urls = [
        "https://github.com/google/re2/archive/506cfa4bffd060c06ec338ce50ea3468daa6c814.tar.gz",
    ],
)

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

#tensorflow. Commit from September 21, 2020 making net_http client API visible
http_archive(
    name = "com_google_tensorflow_serving",
    sha256 = "d2d5874a6f65dbca25f8750c03b1d64d46995e04561b83c1bd4b0b956ebf96ef",
    strip_prefix = "serving-72927141f29d83614cf8304725b1a7e6357ce6ac",
    urls = ["https://github.com/tensorflow/serving/archive/72927141f29d83614cf8304725b1a7e6357ce6ac.zip"],
)

load("@com_google_tensorflow_serving//tensorflow_serving:workspace.bzl", "tf_serving_workspace")

tf_serving_workspace()

#jsoncpp. Official release 1.9.2.
http_archive(
    name = "com_jsoncpp",
    build_file = "@//ecclesia/oss:jsoncpp.BUILD",
    sha256 = "77a402fb577b2e0e5d0bdc1cf9c65278915cdb25171e3452c68b6da8a561f8f0",
    strip_prefix = "jsoncpp-1.9.2",
    urls = ["https://github.com/open-source-parsers/jsoncpp/archive/1.9.2.tar.gz"],
)

http_archive(
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = ["https://zlib.net/zlib-1.2.11.tar.gz"],
)

http_archive(
    name = "boringssl",
    sha256 = "66e1b0675d58b35f9fe3224b26381a6d707c3293eeee359c813b4859a6446714",
    strip_prefix = "boringssl-9b7498d5aba71e545747d65dc65a4d4424477ff0",
    urls = [
        "https://github.com/google/boringssl/archive/9b7498d5aba71e545747d65dc65a4d4424477ff0.tar.gz",
    ],
)

http_archive(
    name = "ncurses",
    build_file = "@//ecclesia/oss:ncurses.BUILD",
    sha256 = "aa057eeeb4a14d470101eff4597d5833dcef5965331be3528c08d99cebaa0d17",
    strip_prefix = "ncurses-6.1",
    urls = [
        "http://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.1.tar.gz"
    ],
)

http_archive(
    name = "libedit",
    build_file = "@//ecclesia/oss:libedit.BUILD",
    sha256 = "dbb82cb7e116a5f8025d35ef5b4f7d4a3cdd0a3909a146a39112095a2d229071",
    strip_prefix = "libedit-20191231-3.1",
    urls = [
        "https://www.thrysoee.dk/editline/libedit-20191231-3.1.tar.gz",
    ],
)

http_archive(
    name = "ipmitool",
    build_file = "@//ecclesia/oss:ipmitool.BUILD",
    sha256 = "e93fe5966d59e16bb4317c9c588cdf35d6100753a0ba957c493b064bcad52493",
    strip_prefix = "ipmitool-1.8.18",
    urls = [
        "https://github.com/ipmitool/ipmitool/releases/download/IPMITOOL_1_8_18/ipmitool-1.8.18.tar.gz"
    ],
    patches = [
        # Openssl 1.1 made struct EVP_MD_CTX opaque, so we have to heap
        # allocate it.
        "//ecclesia/oss:ipmitool.patches/ipmitool.lanplus_crypt_impl.patch"
    ],
    patch_cmds = [
        "./configure CFLAGS=-fPIC CXXFLAGS=-fPIC --enable-shared=no",
        "cp ./config.h include",
    ],
)

http_archive(
    name = "curl",
    build_file = "@//ecclesia/oss:curl.BUILD",
    sha256 = "01ae0c123dee45b01bbaef94c0bc00ed2aec89cb2ee0fd598e0d302a6b5e0a98",
    strip_prefix = "curl-7.69.1",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/curl.haxx.se/download/curl-7.69.1.tar.gz",
        "https://curl.haxx.se/download/curl-7.69.1.tar.gz",
    ],
)

http_archive(
    name = "jansson",
    build_file = "@//ecclesia/oss:jansson.BUILD",
    sha256 = "5f8dec765048efac5d919aded51b26a32a05397ea207aa769ff6b53c7027d2c9",
    strip_prefix = "jansson-2.12",
    urls = [
        "https://digip.org/jansson/releases/jansson-2.12.tar.gz",
    ],
)

http_archive(
    name = "libredfish",
    build_file = "@//ecclesia/oss:libredfish.BUILD",
    sha256 = "301563b061da5862e2dfa7da367d37298856eace5aabba80cabf15a42b6ed3d3",
    strip_prefix = "libredfish-1.2.8",
    urls = [
        "https://github.com/DMTF/libredfish/archive/1.2.8.tar.gz",
    ],
    patches = [
        "//ecclesia/oss:libredfish.patches/01.redfishService.h.patch",
        "//ecclesia/oss:libredfish.patches/02.queue.h.patch",
        "//ecclesia/oss:libredfish.patches/03.queue.c.patch",
        "//ecclesia/oss:libredfish.patches/04.service.c.patch",
        "//ecclesia/oss:libredfish.patches/05.internal_service.h.patch",
        "//ecclesia/oss:libredfish.patches/06.asyncRaw.c.patch",
        "//ecclesia/oss:libredfish.patches/07.mtls_auth.redfishService.h.patch",
        "//ecclesia/oss:libredfish.patches/08.mtls_auth.asyncRaw.c.patch",
        "//ecclesia/oss:libredfish.patches/09.mtls_auth.internal_service.h.patch",
        "//ecclesia/oss:libredfish.patches/10.mtls_auth.service.c.patch",
    ],
)

http_archive(
    name = "redfishMockupServer",
    build_file = "@//ecclesia/oss:redfishMockupServer.BUILD",
    sha256 = "2a4663441b205189686dfcc9a4082fc8c30cb1637126281fb291ad69cabb43f9",
    strip_prefix = "Redfish-Mockup-Server-1.1.0",
    urls = ["https://github.com/DMTF/Redfish-Mockup-Server/archive/1.1.0.tar.gz"],
    patches = [
        "//ecclesia/oss:redfishMockupServer.patches/01.remove_grequest.patch",
        "//ecclesia/oss:redfishMockupServer.patches/02.add_ipv6.patch",
        "//ecclesia/oss:redfishMockupServer.patches/03.fix_traversal_vulnerability.patch",
        "//ecclesia/oss:redfishMockupServer.patches/04.add_uds.patch",
        "//ecclesia/oss:redfishMockupServer.patches/05.add_session_auth_support.patch",
        "//ecclesia/oss:redfishMockupServer.patches/06.add_mtls_support.patch",
        "//ecclesia/oss:redfishMockupServer.patches/07.add_eventservice_support.patch",
    ],
)

http_archive(
    name = "libsodium",
    build_file = "@//ecclesia/oss:libsodium.BUILD",
    sha256 = "d59323c6b712a1519a5daf710b68f5e7fde57040845ffec53850911f10a5d4f4",
    strip_prefix = "libsodium-1.0.18",
    urls = ["https://github.com/jedisct1/libsodium/archive/1.0.18.tar.gz"],
    patches = [
      "//ecclesia/oss:libsodium.patches/libsodium.01.version_h.patch",
    ]
)

http_archive(
    name = "zeromq",
    build_file = "@//ecclesia/oss:zeromq.BUILD",
    sha256 = "27d1e82a099228ee85a7ddb2260f40830212402c605a4a10b5e5498a7e0e9d03",
    strip_prefix = "zeromq-4.2.1",
    urls = ["https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz"],
    patches = [
      "//ecclesia/oss:zeromq.patches/zmq.01.add_platform_hpp.patch"
    ],
)

http_archive(
    name = "cppzmq",
    build_file = "@//ecclesia/oss:cppzmq.BUILD",
    sha256 = "9853e0437d834cbed5d3c223bf1d755cadee70e7c964c6e42c4c6783dee5d02c",
    strip_prefix = "cppzmq-4.7.1",
    urls = ["https://github.com/zeromq/cppzmq/archive/v4.7.1.tar.gz"],
)

# Riegeli. Uses the latest commit as of Mar 17, 2021.
http_archive(
    name = "com_google_riegeli",
    url = "https://github.com/google/riegeli/archive/9c3f3203ad04a45fe8743bb71cd0cd98c76e394d.tar.gz",
    sha256 = "8f28ca19b1ebe96df6c1d76ecadf1aa4e7fcf151c0492e91b7401a47ce2add62",
    strip_prefix = "riegeli-9c3f3203ad04a45fe8743bb71cd0cd98c76e394d",
)
# Additional projects needed by riegeli.
http_archive(
    name = "org_brotli",
    sha256 = "6e69be238ff61cef589a3fa88da11b649c7ff7a5932cb12d1e6251c8c2e17a2f",
    strip_prefix = "brotli-1.0.7",
    urls = ["https://github.com/google/brotli/archive/v1.0.7.zip"],
)
http_archive(
    name = "net_zstd",
    build_file = "@com_google_riegeli//third_party:net_zstd.BUILD",
    sha256 = "b6c537b53356a3af3ca3e621457751fa9a6ba96daf3aebb3526ae0f610863532",
    strip_prefix = "zstd-1.4.5/lib",
    urls = ["https://github.com/facebook/zstd/archive/v1.4.5.zip"],
)
http_archive(
    name = "highwayhash",
    build_file = "@com_google_riegeli//third_party:highwayhash.BUILD",
    sha256 = "cf891e024699c82aabce528a024adbe16e529f2b4e57f954455e0bf53efae585",
    strip_prefix = "highwayhash-276dd7b4b6d330e4734b756e97ccfb1b69cc2e12",
    urls = ["https://github.com/google/highwayhash/archive/276dd7b4b6d330e4734b756e97ccfb1b69cc2e12.zip"],
)
