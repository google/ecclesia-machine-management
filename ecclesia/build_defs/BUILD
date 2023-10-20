load("@bazel_skylib//:bzl_library.bzl", "bzl_library")

licenses(["notice"])

# This file is exported for use by shar_binary rules.
exports_files(
    srcs = ["shar_stub.cc.template"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "embed",
    srcs = ["embed.bzl"],
    visibility = ["//visibility:public"],
    deps = [":file"],
)

bzl_library(
    name = "file",
    srcs = ["file.bzl"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "constants",
    srcs = ["constants.bzl"],
    visibility = ["//visibility:public"],
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

bzl_library(
    name = "redpath_query",
    srcs = ["redpath_query.bzl"],
)

bzl_library(
    name = "proto",
    srcs = ["proto.bzl"],
    visibility = ["//visibility:public"],
)

bzl_library(
    name = "resources",
    srcs = ["resources.bzl"],
    visibility = ["//ecclesia:mmanager_users"],
)
