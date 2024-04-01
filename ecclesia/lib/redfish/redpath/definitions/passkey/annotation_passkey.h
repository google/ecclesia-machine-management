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

#ifndef ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_PASSKEY_ANNOTATION_PASSKEY_H_
#define ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_PASSKEY_ANNOTATION_PASSKEY_H_

namespace ecclesia {
// An empty class that is used to add url annotations to subqueries. This class
// will have limited visibility, forcing users that want to have url annotations
// must add themselves to the visibility of this class, allowing us to regulate
// and identify who is accessing it.
class RedfishAnnotationsPasskey {
  friend class RedfishAnnotationsPasskeyFactory;

 private:
  // Make constructor private to prevent uniform initialization.
  RedfishAnnotationsPasskey() = default;
};

class RedfishAnnotationsPasskeyFactory {
 public:
  static RedfishAnnotationsPasskey GetPassKey() {
    return RedfishAnnotationsPasskey();
  }
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_REDPATH_DEFINITIONS_PASSKEY_ANNOTATION_PASSKEY_H_
