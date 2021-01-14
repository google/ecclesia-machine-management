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

#ifndef ECCLESIA_MAGENT_LIB_EVENT_READER_MCED_READER_H_
#define ECCLESIA_MAGENT_LIB_EVENT_READER_MCED_READER_H_

#include <sys/socket.h>

#include <cstdio>
#include <queue>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "ecclesia/magent/lib/event_reader/event_reader.h"

namespace ecclesia {

// An interface for encapsulating the libc functions used to implement the
// underlying socket operations in the reader. This allows for testing via
// dependency injection.
//
// Note that it's very important that the daemon reader direct all libc calls
// through this interface; taking descriptors or FILE* and feeding them directly
// into other libc calls will do terrible things in testing where the objects
// and values produced will not actually be real descriptors and files. If
// future changes need to use additional libc calls then the interface must be
// extended to add them.
class McedaemonSocketInterface {
 public:
  McedaemonSocketInterface() {}
  virtual ~McedaemonSocketInterface() = default;

  virtual int CallSocket(int domain, int type, int protocol) = 0;
  virtual FILE *CallFdopen(int fd, const char *mode) = 0;
  virtual char *CallFgets(char *s, int size, FILE *stream) = 0;
  virtual int CallFclose(FILE *stream) = 0;
  virtual int CallConnect(int sockfd, const struct sockaddr *addr,
                          socklen_t addrlen) = 0;
  virtual int CallClose(int fd) = 0;
};

// An implementation of the socket interface that forwards everything to libc.
class LibcMcedaemonSocket final : public McedaemonSocketInterface {
 public:
  int CallSocket(int domain, int type, int protocol) override;
  FILE *CallFdopen(int fd, const char *mode) override;
  char *CallFgets(char *s, int size, FILE *stream) override;
  int CallFclose(FILE *stream) override;
  int CallConnect(int sockfd, const struct sockaddr *addr,
                  socklen_t addrlen) override;
  int CallClose(int fd) override;
};

// A reader class to reading machine check exceptions from the mcedaemon
// (https://github.com/thockin/mcedaemon)
class McedaemonReader : public SystemEventReader {
 public:
  // Input is the path to the unix domain socket to talk to the mcedaemon
  McedaemonReader(std::string mced_socket_path,
                  McedaemonSocketInterface *socket_intf);

  absl::optional<SystemEventRecord> ReadEvent() override {
    absl::MutexLock l(&mces_lock_);
    if (mces_.empty()) return absl::nullopt;
    auto event = std::move(mces_.front());
    mces_.pop();
    return event;
  }

  ~McedaemonReader() {
    // Signal the reader loop to exit
    exit_loop_.Notify();
    reader_loop_.join();
  }

 private:
  // The reader loop. Polls for mces from the mcedaemon and logs them in mces_.
  // Runs as a seperate thread.
  void Loop();

  std::string mced_socket_path_;
  McedaemonSocketInterface *socket_intf_;

  absl::Mutex mces_lock_;
  std::queue<SystemEventRecord> mces_ ABSL_GUARDED_BY(mces_lock_);
  absl::Notification exit_loop_;
  std::thread reader_loop_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_MAGENT_LIB_EVENT_READER_MCED_READER_H_
