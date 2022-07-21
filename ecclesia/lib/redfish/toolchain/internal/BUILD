"""Tools and libraries for Redfish client interfaces."""

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")
load("//ecclesia/lib/redfish/toolchain:accessors.bzl", "cc_redfish_accessor_library")

licenses(["notice"])

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
    csdl_files = [
        "test/csdl_in/ConcertService_v1.xml",
        "test/csdl_in/ConductorCollection_v1.xml",
        "test/csdl_in/InstrumentCollection_v1.xml",
        "test/csdl_in/Instrument_v1.xml",
        "test/csdl_in/ServiceRoot_v1.xml",
        "test/csdl_in/VenueCollection_v1.xml",
    ],
    redfish_profile = "test/profiles/simple_profile.json",
)

cc_redfish_accessor_library(
    name = "redfish_accessors",
    csdl_files = ["test/csdl_in/Assembly_v1.xml"],
    redfish_profile = "test/profiles/Assembly.v1_0_0.json",
)

cc_test(
    name = "redfish_accessors_test",
    srcs = ["redfish_accessors_test.cc"],
    deps = [
        ":redfish_accessors",
        "//ecclesia/lib/redfish/testing:json_mockup",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
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
        [
            "test/csdl_in/ConcertService_v1.xml",
            "test/csdl_in/ConductorCollection_v1.xml",
            "test/csdl_in/InstrumentCollection_v1.xml",
            "test/csdl_in/Instrument_v1.xml",
            "test/csdl_in/ServiceRoot_v1.xml",
            "test/csdl_in/VenueCollection_v1.xml",
            "test/profiles/simple_profile.json",
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
    visibility = ["//visibility:public"],
    deps = [
        ":profile_to_descriptor",
        "@com_google_protobuf//:protobuf_python",
    ],
)
