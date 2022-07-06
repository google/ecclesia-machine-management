"""Tools and libraries for Redfish client interfaces."""

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")
load("//ecclesia/lib/redfish/toolchain:accessors.bzl", "cc_redfish_accessor_library")

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
    visibility = ["//visibility:public"],
)

py_proto_library(
    name = "descriptor_py_pb2",
    visibility = [":__subpackages__"],
    deps = [":descriptor_proto"],
)

cc_redfish_accessor_library(
    name = "descriptor_sample",
    csdl_files = glob(["test/csdl_in/*.xml"]),
    redfish_profile = "test/simple_profile.json",
)

py_library(
    name = "profile_to_descriptor",
    srcs = ["profile_to_descriptor.py"],
    deps = [
        ":descriptor_py_pb2",
        "@com_google_absl_py//absl/logging",
    ],
)

py_test(
    name = "profile_to_descriptor_test",
    srcs = ["profile_to_descriptor_test.py"],
    data =
        glob([
            "test/csdl_in/**",
        ]) + [
            "test/simple_profile.json",
            "test/simple_profile_descriptor.textproto",
        ],
    deps = [
        ":descriptor_py_pb2",
        ":profile_to_descriptor",
        "//ecclesia/lib/protobuf:compare",
        "@com_google_absl_py//absl/testing:absltest",
        "@com_google_protobuf//:protobuf_python",
    ],
)

py_binary(
    name = "profile_to_descriptor_main",
    srcs = ["profile_to_descriptor_main.py"],
    deps = [
        ":profile_to_descriptor",
        "@com_google_protobuf//:protobuf_python",
    ],
)
