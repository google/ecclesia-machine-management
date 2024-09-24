"""Helper macro for defining resource protos"""

load("//devtools/build_cleaner/skylark:build_defs.bzl", "register_extension_info")
load("//tools/build_defs/go:go_proto_library.bzl", "go_proto_library")
load("//tools/build_defs/proto/cpp:cc_proto_library.bzl", "cc_proto_library")

proto_library = native.proto_library

_DEFAULT_VISIBILITY = ["//ecclesia:mmanager_users"]

def resource_proto(resource, name, srcs, deps = None):
    """Generates a standardized set of proto libraries for a given MManager Resource.

    <name>_proto    - proto_library
    <name>_cc_proto - cc_proto_library

    Args:
      name: Name of the resulting proto_library (expects '_proto' suffix, and will add it if not
          provided).
      srcs: Sources for the resulting proto_library.
      deps: Additional dependencies for the resulting proto_library.
    """

    # Fail if name was provided with '_proto' suffix. '_proto' suffix is added automatically via
    # this macro and it should not be specified as part of the name arguement.
    if name.endswith("_proto"):
        fail("Resource proto macro should not be called with name attribute that ends with '_proto'.")

    if deps == None:
        deps = []
    proto_library(
        name = name + "_proto",
        srcs = srcs,
        exports = ["//platforms/ecclesia/mmanager/service/resource/" + resource + ":" + name + "_proto"],
        deps = deps,
        visibility = _DEFAULT_VISIBILITY,
    )

    native.cc_proto_library(
        name = name + "_cc_proto",
        deps = [":" + name + "_proto"],
        visibility = _DEFAULT_VISIBILITY,
    )
    go_proto_library(
        name = name + "_go_proto",
        deps = [":" + name + "_proto"],
        visibility = _DEFAULT_VISIBILITY,
    )

    native.py_proto_library(
        name = name + "_py_pb2",
        deps = [":" + name + "_proto"],
        visibility = _DEFAULT_VISIBILITY,
    )

register_extension_info(
    extension = resource_proto,
    label_regex_for_dep = "{extension_name}_proto",
)
