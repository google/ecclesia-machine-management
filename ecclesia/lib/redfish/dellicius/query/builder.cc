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

#include "ecclesia/lib/redfish/dellicius/query/builder.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "ecclesia/lib/file/path.h"

namespace platforms_redfish {

namespace {

constexpr absl::string_view kHeaderFileTemplate = R"(
#ifndef $0
#define $0

$1

#endif  // $0
)";

std::string ToSnakeCase(absl::string_view input) {
  bool was_not_underscore = false;  // Initialize to false for case 1 (below)
  bool was_not_cap = false;
  std::string result;
  result.reserve(input.size() << 1);

  for (size_t i = 0; i < input.size(); ++i) {
    if (absl::ascii_isupper(input[i])) {
      // Consider when the current character B is capitalized:
      // 1) At beginning of input:   "B..." => "b..."
      //    (e.g. "Biscuit" => "biscuit")
      // 2) Following a lowercase:   "...aB..." => "...a_b..."
      //    (e.g. "gBike" => "g_bike")
      // 3) At the end of input:     "...AB" => "...ab"
      //    (e.g. "GoogleLAB" => "google_lab")
      // 4) Followed by a lowercase: "...ABc..." => "...a_bc..."
      //    (e.g. "GBike" => "g_bike")
      if (was_not_underscore &&                     //            case 1 out
          (was_not_cap ||                           // case 2 in, case 3 out
           (i + 1 < input.size() &&                 //            case 3 out
            absl::ascii_islower(input[i + 1])))) {  // case 4 in
        // We add an underscore for case 2 and case 4.
        result.push_back('_');
      }
      result.push_back(absl::ascii_tolower(input[i]));
      was_not_underscore = true;
      was_not_cap = false;
    } else {
      result.push_back(input[i]);
      was_not_underscore = input[i] != '_';
      was_not_cap = true;
    }
  }
  return result;
}

absl::string_view::difference_type FindIgnoreCase(
    absl::string_view haystack, absl::string_view needle,
    absl::string_view::size_type pos = 0) {
  if (pos > haystack.size())
    return static_cast<int64_t>(absl::string_view::npos);
  // We use the cursor to iterate through the haystack...on each
  // iteration the cursor is moved forward one character.
  absl::string_view cursor = haystack.substr(pos);
  while (cursor.size() >= needle.size()) {
    if (absl::StartsWithIgnoreCase(cursor, needle)) {
      return cursor.data() - haystack.data();
    }
    cursor.remove_prefix(1);
  }
  return static_cast<int64_t>(absl::string_view::npos);
}

}  // namespace

absl::Status FileBuilderBase::WriteToFile(absl::string_view filename,
                                          absl::string_view content) const {
  std::string output_file = ecclesia::JoinFilePaths(output_dir_, filename);
  std::fstream out_f(output_file, std::fstream::binary | std::fstream::trunc |
                                      std::fstream::out);
  if (!out_f.is_open()) {
    return absl::InternalError("unable to open " + output_file);
  }

  out_f << content;
  return absl::OkStatus();
}

absl::Status FileBuilderBase::WriteHeaderFile(absl::string_view content) const {
  std::string filename = absl::StrCat(name_snake_case(), ".h");
  // Create header guard string

  // Check if prefix is in directory. If not return failure
  if (FindIgnoreCase(output_dir(), header_path()) == -1) {
    return absl::FailedPreconditionError(
        absl::StrCat("Query is not present in ", header_path(), " folder"));
  }

  std::string header_guard =
      absl::AsciiStrToUpper(absl::StrCat(header_path(), "_", filename, "_"));
  header_guard = absl::StrReplaceAll(header_guard, {{"/", "_"}, {".", "_"}});

  return WriteToFile(
      filename, absl::Substitute(kHeaderFileTemplate, header_guard, content));
}

absl::Status FileBuilderBase::WriteSourceFile(absl::string_view content) const {
  return WriteToFile(absl::StrCat(name_snake_case(), ".cc"), content);
}

std::string FileBuilderBase::name_snake_case() const {
  return ToSnakeCase(name_);
}

}  // namespace platforms_redfish
