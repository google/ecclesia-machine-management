licenses(["notice"])

cc_library(
    name = "clock",
    srcs = ["clock.cc"],
    hdrs = ["clock.h"],
    visibility = ["//ecclesia:library_users"],
    deps = ["@com_google_absl//absl/time"],
)

cc_library(
    name = "clock_fake",
    testonly = True,
    hdrs = ["clock_fake.h"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":clock",
        "@com_google_absl//absl/time",
    ],
)
