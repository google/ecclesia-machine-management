# Copyright 2020 Google LLC
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

licenses(["notice"])

cc_library(
    name = "thread",
    srcs = ["thread.cc"],
    hdrs = ["thread.h"],
    visibility = ["//ecclesia:library_users"],
)

cc_library(
    name = "thread_mock",
    testonly = 1,
    hdrs = [
        "thread_mock.h",
    ],
    tags = ["nozapfhahn"],
    visibility = ["//ecclesia:library_users"],
    deps = [
        ":thread",
        "@com_google_googletest//:gtest",
    ],
)
