"""OSS Build definitions

These macros wrap common build rules and allows scripts (i.e. Bulldozer) to
specifically target them with modifications to be usable in open source.
"""

# load("@rules_cc//cc:cc_binary.bzl", _cc_binary = "cc_binary")
# load("@rules_cc//cc:cc_test.bzl", _cc_test = "cc_test")

_cc_binary = native.cc_binary
_cc_test = native.cc_test

def ecclesia_oss_static_linked_cc_binary(*args, **kwargs):
    """ecclesia_oss_static_linked_cc_binary requires Builddozer to add libc

    Static libc linkopts are required to make the build truly static. These
    linkopts are added by Buildozer to these build rules.
    """
    _cc_binary(*args, **kwargs)

def ecclesia_benchmark_cc_test(*args, **kwargs):
    """ecclesia_benchmark_cc_test wraps cc_test targets for benchmarks.

    This is done using a wrapper rule to allow for benchmarks to be easily
    distinguished from regular tests by looking at the type.
    """
    _cc_test(*args, **kwargs)
