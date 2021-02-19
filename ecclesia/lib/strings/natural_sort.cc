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

#include "ecclesia/lib/strings/natural_sort.h"

#include <cstddef>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

int NaturalSortCmp(absl::string_view lhs, absl::string_view rhs) {
  size_t index = 0;
  while ((index < lhs.size()) && (index < rhs.size())) {
    if (absl::ascii_isdigit(lhs[index]) && absl::ascii_isdigit(rhs[index])) {
      // Both have a sequence of numbers so compare by overall value
      size_t lhs_seq_length = 0, rhs_seq_length = 0;
      while (absl::ascii_isdigit(lhs[index + lhs_seq_length])) lhs_seq_length++;
      while (absl::ascii_isdigit(rhs[index + rhs_seq_length])) rhs_seq_length++;

      int lhs_num = -1, rhs_num = -1;
      Check(absl::SimpleAtoi(lhs.substr(index, lhs_seq_length), &lhs_num) &&
                absl::SimpleAtoi(rhs.substr(index, rhs_seq_length), &rhs_num),
            "Converting the series of digits to integers")
          << "Failed to convert these sequences into integers: "
          << lhs.substr(index, lhs_seq_length) << " and "
          << rhs.substr(index, rhs_seq_length);

      if (lhs_num != rhs_num) {
        return lhs_num - rhs_num;
      } else if (lhs_seq_length != rhs_seq_length) {
        // Only case when there are leading zeroes, in which case we want the
        // one with more leading zeroes to be less (longer sequence)
        return rhs_seq_length - lhs_seq_length;
      }

      // Both sets of numbers are the same so skip ahead in index
      index += lhs_seq_length;  // Equal to rhs_seq_length
    } else if (lhs[index] < rhs[index]) {
      return -1;
    } else if (rhs[index] < lhs[index]) {
      return 1;
    } else {
      // Equal so far
      index++;
    }
  }

  if (index < lhs.size()) {
    // LHS string is longer
    return 1;
  }
  if (index < rhs.size()) {
    // RHS string is longer
    return -1;
  }
  return 0;
}

}  // namespace ecclesia
