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

#include "ecclesia/magent/lib/ipmi/interface_options.h"

#include <fcntl.h>
#include <unistd.h>

#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "absl/cleanup/cleanup.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/magent/proto/config.pb.h"

namespace ecclesia {

absl::optional<ecclesia::MagentConfig::IpmiCredential> GetIpmiCredentialFromPb(
    const char *path) {
  ecclesia::MagentConfig magent_config;

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    WarningLog() << "Fail to open " << path;
    return absl::nullopt;
  }
  auto fd_closer = absl::MakeCleanup([fd]() { close(fd); });

  google::protobuf::io::FileInputStream input(fd);
  if (!magent_config.ParseFromZeroCopyStream(&input)) {
    WarningLog() << "Fail to parse " << path;
    return absl::nullopt;
  }

  return magent_config.ipmi_cred();
}

}  // namespace ecclesia
