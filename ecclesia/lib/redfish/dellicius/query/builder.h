/*
 * Copyright 2023 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_DELLICIUS_QUERY_BUILDER_H_
#define ECCLESIA_LIB_REDFISH_DELLICIUS_QUERY_BUILDER_H_

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace platforms_redfish {

// Base class for creating Header and Source files for redfish redpath query
// test generators.
class FileBuilderBase {
 public:
  FileBuilderBase(absl::string_view name, absl::string_view output_dir,
                  absl::string_view header_path)
      : name_(name), output_dir_(output_dir), header_path_(header_path) {}

  virtual ~FileBuilderBase() = default;

  // Builder must override this function to create all the files necessary.
  virtual absl::Status WriteFiles() const = 0;

  // Helper functions to create a custom file, header and source files.
  absl::Status WriteToFile(absl::string_view filename,
                           absl::string_view content) const;
  absl::Status WriteHeaderFile(absl::string_view content) const;
  absl::Status WriteSourceFile(absl::string_view content) const;

  absl::string_view name() const { return name_; }
  std::string name_snake_case() const;

  absl::string_view output_dir() const { return output_dir_; }
  absl::string_view header_path() const { return header_path_; }

 private:
  const std::string name_;
  const std::string output_dir_;
  const std::string header_path_;
};

}  // namespace platforms_redfish

#endif  // ECCLESIA_LIB_REDFISH_DELLICIUS_QUERY_BUILDER_H_
