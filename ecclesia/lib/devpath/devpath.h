/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This library provides a collection of helper functions for manipulating
// devpath strings in a rigorous way. The devpaths are assumed to be ones that
// follow the Ecclesia devpath format (go/ecclesia-devpath), i.e. "bare paths".
//
// While the library operates on strings and string_views, it is important to
// note that not all such objects constitute valid devpaths. The IsValidDevpath
// function is provided to test if a given arbitrary string is such a path. All
// other functions in this library ASSUME that the paths they are given are
// valid devpaths and have undefined behavior if they are passed a string which
// is not a devpath. If you have strings coming from an external data source
// which you are not absolutely, 100% certain are valid devpaths, then test them
// with IsValidDevpath before passing them to any of the helper functions.

#ifndef ECCLESIA_LIB_DEVPATH_DEVPATH_H_
#define ECCLESIA_LIB_DEVPATH_DEVPATH_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace ecclesia {

// Returns a boolean indicating if a given path is a valid devpath.
//
// Valid simply means that the given string follows the defined devpath format.
// A valid path still may not correspond to that produced by any known hardware.
bool IsValidDevpath(absl::string_view path);

// Given a devpath, return the three devpath components: path, suffix namespace
// and suffix text. The two suffix components may be empty but the path never
// will be.
struct DevpathComponents {
  absl::string_view path;
  absl::string_view suffix_namespace;
  absl::string_view suffix_text;
};
DevpathComponents GetDevpathComponents(absl::string_view path);

// Get the devpath of the plugin that the given path belongs to. Note that if
// the given path is a plugin, the result is just itself.
absl::string_view GetDevpathPlugin(absl::string_view path);

// Given a devpath, get its upstream devpath. More specifically:
// * For a plugin/cable, produces the devpath of the upstream connector
// * For a connector or device, produces the devpath of the plugin it resides on
// If the devpath does not have an upstream, nullopt is returned (i.e. root
// board). Note the limitation of this method is that it can only provide a
// single upstream devpath; if the system model in reality has multiple upstream
// devpaths for a given devpath, this method is unable to provide that info.
std::optional<std::string> GetUpstreamDevpath(absl::string_view devpath);

// Given a set of devpath components, construct a full devpath.
std::string MakeDevpathFromComponents(const DevpathComponents &components);

// Given a devpath, returns if the devpath corresponds to a plugin.
bool IsPluginDevpath(absl::string_view devpath);

};  // namespace ecclesia

#endif  // ECCLESIA_LIB_DEVPATH_DEVPATH_H_
