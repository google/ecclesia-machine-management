load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

# This file is exported for use by shar_binary rules.
exports_files(
    srcs = ["shar_stub.cc.template"],
    visibility = ["//ecclesia:library_users"],
)

bzl_library(
    name = "embed",
    srcs = ["embed.bzl"],
    visibility = ["//ecclesia:library_users"],
    deps = [":file"],
)

bzl_library(
    name = "file",
    srcs = ["file.bzl"],
)

bzl_library(
    name = "constants",
    srcs = ["constants.bzl"],
    deps = [":file"],
)

bzl_library(
    name = "oss",
    srcs = ["oss.bzl"],
)

bzl_library(
    name = "query",
    srcs = ["query.bzl"],
)
