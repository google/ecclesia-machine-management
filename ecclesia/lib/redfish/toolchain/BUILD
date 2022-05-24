"""Tools and libraries for Redfish client interfaces."""

load("//ecclesia/build_defs:proto.bzl", "proto_data")

licenses(["notice"])

py_library(
    name = "profile_to_csdl",
    srcs = ["profile_to_csdl.py"],
)

py_binary(
    name = "profile_to_csdl_main",
    srcs = ["profile_to_csdl_main.py"],
    deps = [
        ":profile_to_csdl",
    ],
)

py_test(
    name = "profile_to_csdl_test",
    srcs = ["profile_to_csdl_test.py"],
    data =
        glob([
            "test/csdl_in/**",
            "test/csdl_out/**",
        ]) + ["test/simple_profile.json"],
    deps = [
        ":profile_to_csdl",
        "@com_google_absl_py//absl/testing:absltest",
    ],
)

proto_library(
    name = "descriptor_proto",
    srcs = ["descriptor.proto"],
)

proto_data(
    name = "descriptor_proto_data",
    src = "test/simple_profile_descriptor.textproto",
    out = "test/simple_profile_descriptor.binarypb",
    proto_deps = [":descriptor_proto"],
    proto_name = "ecclesia.Profile",
)
