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

#include "ecclesia/magent/lib/event_reader/mced_reader.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <optional>
#include <queue>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"
#include "ecclesia/lib/logging/posix.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"
#include "re2/re2.h"

namespace ecclesia {
namespace {

// Limit on the maximum line length we should expect from mced.
constexpr int kMcedMaxLineLength = 1024;
constexpr absl::Duration kRetryDelay = absl::Seconds(10);

// Given a path to the unix domain socket, return a file stream to read mces
// from
FILE *InitSocket(const std::string &socket_path,
                 McedaemonSocketInterface *socket_intf) {
  // Open a unix domain socket.
  int socket_fd = socket_intf->CallSocket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    PosixErrorLog() << "Failed opening socket to mced";
    return nullptr;
  }
  // Connect to mced via it's published socket path
  struct sockaddr_un remote;
  remote.sun_family = AF_UNIX;
  strncpy(remote.sun_path, socket_path.c_str(), sizeof(remote.sun_path));
  if (socket_intf->CallConnect(socket_fd,
                               reinterpret_cast<struct sockaddr *>(&remote),
                               sizeof(remote)) == -1) {
    PosixErrorLog() << "Failed to connect to mced.";
    socket_intf->CallClose(socket_fd);
    return nullptr;
  }
  // Construct a file stream from the file descriptor
  FILE *socket_file = socket_intf->CallFdopen(socket_fd, "r");
  if (!socket_file) {
    PosixErrorLog() << "error during fdopen().";
    socket_intf->CallClose(socket_fd);
    return nullptr;
  }
  ErrorLog() << "Connected successfully to mced.";
  return socket_file;
}

// Determine the type that the included RE2 library uses for string view
// parameters by seeing what the type the QuoteMeta function takes.
template <typename T>
struct ArgExtractor;

template <typename T>
struct ArgExtractor<std::string(T)> {
  using type = T;
};
using StringViewType =
    std::decay_t<typename ArgExtractor<decltype(RE2::QuoteMeta)>::type>;

// This function is mostly borrowed from
// third_party/mosys/modules/architecture/common/mce/mced.cc
// Parses a line of text obtained from the mcedaemon into the MachineCheck
// structure
std::optional<MachineCheck> ParseLine(absl::string_view mced_line) {
  MachineCheck mce;
  char type;
  std::string value_str;
  // Parse only for KERNEL_MCE_V2, since all of the OSes we plan to run on will
  // support this newer version
  static constexpr LazyRE2 mce_pattern = {"%(\\w)=(\\S+)"};

  StringViewType line_view = mced_line;
  while (RE2::FindAndConsume(&line_view, *mce_pattern, &type, &value_str)) {
    // Value can be either signed or unsigned depending on the type.
    uint64_t unsigned_value = 0;
    int64_t signed_value = 0;
    static constexpr LazyRE2 num_pattern = {"(.*)"};
    bool unsigned_status =
        RE2::FullMatch(value_str, *num_pattern, RE2::CRadix(&unsigned_value));
    bool signed_status =
        RE2::FullMatch(value_str, *num_pattern, RE2::CRadix(&signed_value));
    if (!(unsigned_status || signed_status)) continue;

    switch (type) {
      case 'c':
        mce.cpu = signed_value;
        break;
      case 'S':
        mce.socket = signed_value;
        break;
      case 'v':
        mce.vendor = signed_value;
        break;
      case 'A':
        mce.cpuid_eax = unsigned_value;
        break;
      case 'p':
        mce.init_apic_id = unsigned_value;
        break;
      case 'b':
        mce.bank = unsigned_value;
        break;
      case 's':
        mce.mci_status = unsigned_value;
        break;
      case 'a':
        mce.mci_address = unsigned_value;
        break;
      case 'm':
        mce.mci_misc = unsigned_value;
        break;
      case 'y':
        mce.mci_synd = unsigned_value;
        break;
      case 'i':
        mce.mci_ipid = unsigned_value;
        break;
      case 'g':
        mce.mcg_status = unsigned_value;
        break;
      case 'G':
        mce.mcg_cap = unsigned_value;
        break;
      case 't':
        mce.time = absl::FromUnixMicros(unsigned_value);
        break;
      case 'T':
        mce.tsc = unsigned_value;
        break;
      case 'C':
        mce.cs = unsigned_value;
        break;
      case 'I':
        mce.ip = unsigned_value;
        break;
      case 'B':
        mce.boot = signed_value;
        break;
      default:
        ErrorLog() << "unknown mced key type: 0x" << std::hex
                   << static_cast<int>(type);
    }
  }
  // Sanity check that we parsed a minimum amount of data.
  if (mce.bank.has_value() && mce.mci_status.has_value()) return mce;
  return std::nullopt;
}

// Return value std::nullopt implies error in parsing the mce
std::optional<MachineCheck> ReadOneMce(FILE *socket_file,
                                       McedaemonSocketInterface *socket_intf) {
  char line_buffer[kMcedMaxLineLength];

  if (!socket_intf->CallFgets(line_buffer, kMcedMaxLineLength, socket_file)) {
    PosixErrorLog() << "error reading line from socket_file.";
    return std::nullopt;
  }
  absl::string_view mced_line(line_buffer);
  // Process only valid lines
  if (!absl::StrContains(mced_line, '\n')) return std::nullopt;
  return ParseLine(mced_line);
}

}  // namespace

int LibcMcedaemonSocket::CallSocket(int domain, int type, int protocol) {
  return socket(domain, type, protocol);
}
FILE *LibcMcedaemonSocket::CallFdopen(int fd, const char *mode) {
  return fdopen(fd, mode);
}
char *LibcMcedaemonSocket::CallFgets(char *s, int size, FILE *stream) {
  return fgets(s, size, stream);
}
int LibcMcedaemonSocket::CallFclose(FILE *stream) { return fclose(stream); }
int LibcMcedaemonSocket::CallConnect(int sockfd, const struct sockaddr *addr,
                                     socklen_t addrlen) {
  return connect(sockfd, addr, addrlen);
}
int LibcMcedaemonSocket::CallClose(int fd) { return close(fd); }

McedaemonReader::McedaemonReader(std::string mced_socket_path,
                                 McedaemonSocketInterface *socket_intf)
    : mced_socket_path_(std::move(mced_socket_path)),
      socket_intf_(socket_intf),
      reader_loop_(&McedaemonReader::Loop, this) {}

// Scan for MCEs from the mcedaemon and log them into mces_
void McedaemonReader::Loop() {
  do {
    // Open a socket and get a file stream to read mces from
    if (FILE *socket_file = InitSocket(mced_socket_path_, socket_intf_)) {
      std::optional<MachineCheck> mce;
      while ((mce = ReadOneMce(socket_file, socket_intf_))) {
        absl::MutexLock l(&mces_lock_);
        mces_.push({.record = mce.value()});
      }
      socket_intf_->CallFclose(socket_file);
    }
  } while (!exit_loop_.WaitForNotificationWithTimeout(kRetryDelay));
}

}  // namespace ecclesia
