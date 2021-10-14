licenses(["notice"])

cc_library(
    name = "clock",
    srcs = ["clock.cc"],
    hdrs = ["clock.h"],
    visibility = ["//visibility:public"],
    deps = ["@com_google_absl//absl/time"],
)

cc_library(
    name = "clock_fake",
    testonly = True,
    hdrs = ["clock_fake.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":clock",
        "@com_google_absl//absl/time",
    ],
)
