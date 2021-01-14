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

// Various utility functions for doing network-related operations in testing.

#ifndef ECCLESIA_LIB_NETWORK_TESTING_H_
#define ECCLESIA_LIB_NETWORK_TESTING_H_

namespace ecclesia {

// Utility function that will find an available TCP port to use, or which will
// terminate with a fatal log if it cannot.
//
// This function is not thread-safe. It also cannot guarantee that no other
// thread or process will come along and grab the available port before you use
// it. Therefore it is really only reasonable to use in controlled environments
// where you can have some assurance that these sort of things will not be a
// problem. This is why the function is test-only.
int FindUnusedPortOrDie();

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_NETWORK_TESTING_H_
