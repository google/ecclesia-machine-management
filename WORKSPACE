workspace(name = "com_google_ecclesia")

# First load all the repositories we need.
load("//ecclesia/build_defs:deps_first.bzl", "ecclesia_deps_first")

ecclesia_deps_first()

# Some of these repositories have dependencies of their own,
# with getter methods. We need to call these from a separate
# bazel file, since there are bazel load statements that fail
# before the above initial load is complete.
load("//ecclesia/build_defs:deps_second.bzl", "ecclesia_deps_second")

ecclesia_deps_second()

# Must be loaded after grpc_deps().
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()
