"""Tools and libraries for Redfish client interfaces."""

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
