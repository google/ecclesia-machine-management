/*
 * Copyright 2024 Google LLC
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

#include "ecclesia/lib/redfish/redpath/definitions/query_predicates/util.h"

#include <iterator>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/charconv.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
namespace ecclesia {

absl::StatusOr<int> FuzzyStringComparison(absl::string_view lhs,
                                          absl::string_view rhs) {
  const auto *lhs_iter = lhs.cbegin();
  const auto *rhs_iter = rhs.cbegin();
  // Toggles between string and numeric comparison mode.
  bool string_mode = true;
  while (lhs_iter != lhs.end() && rhs_iter != rhs.end()) {
    if (string_mode) {
      const bool l_digit = absl::ascii_isdigit(*lhs_iter);
      const bool r_digit = absl::ascii_isdigit(*rhs_iter);
      // If both characters are digits, proceed to use numeric comparison.
      if (l_digit && r_digit) {
        string_mode = false;
        continue;
      }
      // If only one character is a digit, that side greater.
      if (l_digit) return -1;
      if (r_digit) return +1;
      // Return the difference between the characters if nonzero.
      const int diff = *lhs_iter - *rhs_iter;
      if (diff != 0) return diff;

      // Otherwise, advance both sides to the next character.
      std::advance(lhs_iter, 1);
      std::advance(rhs_iter, 1);
      // Perform numeric comparison
    } else {
      // At this point, we know both lhs and rhs are digits, we consume
      // the consecutive digits from both strings to form numbers, then
      // compare them.
      // absl::from_chars consumes as many chars as possible to parse into a
      // double. It shouldn't be possible at this point to have an error, but we
      // check it for completeness.
      double l_int = 0;
      auto [l_fc_ptr, l_ec] =
          absl::from_chars(&(*lhs_iter), &(*lhs.end()), l_int);
      if (l_ec != std::errc()) {
        return absl::InternalError(absl::StrCat(
            "Error while trying to parse number from string: ", lhs));
      }
      std::advance(lhs_iter, std::distance(lhs_iter, l_fc_ptr));
      double r_int = 0;
      auto [r_fc_ptr, r_ec] =
          absl::from_chars(&(*rhs_iter), &(*rhs.end()), r_int);
      if (r_ec != std::errc()) {
        return absl::InternalError(absl::StrCat(
            "Error while trying to parse number from string: ", rhs));
      }
      std::advance(rhs_iter, std::distance(rhs_iter, r_fc_ptr));
      // If nonzero, return the difference between the numbers.
      const int diff = static_cast<int>(l_int - r_int);
      if (diff != 0) return diff;

      // Otherwise we process the next substrings with string comparison.
      string_mode = true;
    }
  }
  if (rhs_iter == rhs.end() && lhs_iter == lhs.end()) return 0;
  // If strings match up to this point, the one with less characters is greater.
  if (rhs_iter == rhs.end()) return -1;
  return 1;
}

}  // namespace ecclesia
