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

#ifndef ECCLESIA_LIB_TYPES_OVERLOADED_H_
#define ECCLESIA_LIB_TYPES_OVERLOADED_H_

namespace ecclesia {

// See https://en.cppreference.com/w/cpp/utility/variant/visit, recipe 4. This
// allows writing std::visit calls on absl::variants in a nice way.
template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TYPES_OVERLOADED_H_
