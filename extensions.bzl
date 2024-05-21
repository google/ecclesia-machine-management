load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _non_module_dependencies_impl(_ctx):
    http_archive(
        name = "com_google_googleapis",
        sha256 = "979859a238e6626850fee33d30f4240e90e71009786a55a15134df582dbc2dbe",
        strip_prefix = "googleapis-8d245ac97e058b541f1477b1a85d676b18e80849",
        urls = ["https://github.com/googleapis/googleapis/archive/8d245ac97e058b541f1477b1a85d676b18e80849.tar.gz"],
    )

non_module_dependencies = module_extension(
    implementation = _non_module_dependencies_impl,
)
