"""OSS Build definitions

These macros wrap common build rules and allows scripts (i.e. Bulldozer) to
specifically target them with modifications to be usable in open source.
"""

def ecclesia_oss_static_linked_cc_binary(*args, **kwargs):
    """ecclesia_oss_static_linked_cc_binary requires Builddozer to add libc

    Static libc linkopts are required to make the build truly static. These
    linkopts are added by Buildozer to these build rules.
    """
    native.cc_binary(*args, **kwargs)

def ecclesia_benchmark_cc_test(*args, **kwargs):
    """ecclesia_benchmark_cc_test wraps cc_test targets for benchmarks.

    This is done using a wrapper rule to allow for benchmarks to be easily
    distinguished from regular tests by looking at the type.
    """
    native.cc_test(*args, **kwargs)
