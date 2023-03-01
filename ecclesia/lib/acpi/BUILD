# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@com_google_emboss//:build_defs.bzl", "emboss_cc_library")

package(default_applicable_licenses = [""])

licenses(["notice"])

emboss_cc_library(
    name = "dmar_emb",
    srcs = ["dmar.emb"],
    visibility = ["//visibility:public"],
    deps = [":system_description_table_emb"],
)

cc_library(
    name = "dmar",
    srcs = ["dmar.cc"],
    hdrs = ["dmar.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":dmar_emb",
        ":system_description_table",
        ":system_description_table_emb",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

emboss_cc_library(
    name = "mcfg_emb",
    srcs = ["mcfg.emb"],
    visibility = ["//visibility:public"],
    deps = [":system_description_table_emb"],
)

cc_library(
    name = "mcfg",
    srcs = ["mcfg.cc"],
    hdrs = ["mcfg.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":mcfg_emb",
        ":system_description_table",
        ":system_description_table_emb",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

emboss_cc_library(
    name = "srat_emb",
    srcs = ["srat.emb"],
    visibility = ["//visibility:public"],
    deps = [":system_description_table_emb"],
)

cc_library(
    name = "srat",
    srcs = ["srat.cc"],
    hdrs = ["srat.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":srat_emb",
        ":system_description_table",
        ":system_description_table_emb",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

emboss_cc_library(
    name = "system_description_table_emb",
    srcs = ["system_description_table.emb"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "system_description_table",
    srcs = ["system_description_table.cc"],
    hdrs = ["system_description_table.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":system_description_table_emb",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_emboss//runtime/cpp:cpp_utils",
    ],
)

cc_test(
    name = "dmar_test",
    size = "small",
    srcs = ["dmar_test.cc"],
    data = ["test_data/sys_firmware_acpi_tables_DMAR"],
    deps = [
        ":dmar",
        ":dmar_emb",
        ":system_description_table",
        ":system_description_table_emb",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "mcfg_test",
    size = "small",
    srcs = ["mcfg_test.cc"],
    data = ["test_data/sys_firmware_acpi_tables_MCFG"],
    deps = [
        ":mcfg",
        ":mcfg_emb",
        ":system_description_table",
        ":system_description_table_emb",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "srat_test",
    size = "small",
    srcs = ["srat_test.cc"],
    data = [
        "test_data/sys_firmware_acpi_tables_SRAT",
        "test_data/sys_firmware_acpi_tables_SRAT_v3",
    ],
    deps = [
        ":srat",
        ":srat_emb",
        ":system_description_table",
        ":system_description_table_emb",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "system_description_table_test",
    size = "small",
    srcs = ["system_description_table_test.cc"],
    data = [
        "test_data/sys_firmware_acpi_tables_SRAT",
        "test_data/sys_firmware_acpi_tables_SRAT_bad_length",
        "test_data/sys_firmware_acpi_tables_SRAT_small",
    ],
    deps = [
        ":system_description_table",
        ":system_description_table_emb",
        "//ecclesia/lib/codec:endian",
        "//ecclesia/lib/file:test_filesystem",
        "//ecclesia/lib/testing:status",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_emboss//runtime/cpp:cpp_utils",
        "@com_google_googletest//:gtest_main",
    ],
)
