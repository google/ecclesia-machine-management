#!/bin/bash
#
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

declare -r SERVER="${ECCLESIA_DATA_DIR}/external/redfishMockupServer/redfishMockupServer.par"

# Find the data directory of the mockup, just search for a "redfish" directory
# and take its parent
declare -r DATA_DIR=$(find "${ECCLESIA_DATA_DIR}" -type d -name redfish)/..

# Run the server. Note that -D is already provided. Trying to provide another
# -D argument is undefined behaviour.
chmod u+x "$SERVER"
"$SERVER" -D "$DATA_DIR" "$@"
