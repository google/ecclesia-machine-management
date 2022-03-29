"""Helper macro for defining resource protos"""

_DEFAULT_VISIBILITY = ["//ecclesia:mmanager_users"]

def resource_proto(name, srcs, deps = None):
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

    native.proto_library(
        name = name + "_proto",
        srcs = srcs,
        deps = deps,
        visibility = _DEFAULT_VISIBILITY,
    )

    native.cc_proto_library(
        name = name + "_cc_proto",
        deps = [":" + name + "_proto"],
        visibility = _DEFAULT_VISIBILITY,
    )
