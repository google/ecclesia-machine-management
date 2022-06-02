/*
 * Copyright 2021 Google LLC
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

#ifndef ECCLESIA_LIB_IPMI_IPMI_MANAGER_H_
#define ECCLESIA_LIB_IPMI_IPMI_MANAGER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/arbiter/arbiter.h"
#include "ecclesia/lib/ipmi/ipmi_handle.h"
#include "ecclesia/lib/ipmi/ipmi_interface_params.pb.h"
#include "ecclesia/lib/ipmi/raw/ipmitool.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock.h"

extern "C" {
// Forward declaration so that C header inclusion is in .cc only.
struct ipmi_intf;
}

namespace ecclesia {

// `IpmiManager` is an interface for a global singleton which, upon creation,
// provides an API for attaining IPMI access. This API wraps ipmitool, providing
// a thread-safe interface for IPMI interaction of both the open and lanplus
// interfaces.

// Use `GlobalIpmiManager` to retrieve a reference to the manager singleton.
// This reference can be safely saved for later use.
//
// The calling code can then request exclusive access to IPMI using the
// `Acquire` API. See the comment on that API for details of its use.
// `Acquire` returns an IpmiHandle that represents the exclusive
// access to IPMI and should not be held long term.
//
// The resulting IpmiHandle has both access to the raw interface struct which
// can be used to call IpmiTool commands directly, or utility methods on the
// handle which deliberately abstract away the IpmiTool commands can be used.
//
// Once created via `GlobalIpmiManager`, the IPMI Manager singleton is never
// destroyed, nor is it's destructor called.
//
// Lanplus connections are created by `Acquire` when possible if the connection
// does not already exist. There is no specific connect API.
//
// Lanplus connections will be kept alive via a helper thread which pokes those
// interfaces on a configurable basis. To change that basis, set the
// `ipmi_lanplus_keep_aliver_interval` and/or `ipmi_connection_timeout` flags
// prior to the first call to `GlobalIpmiManager` which constructs the
// `IpmiManager`.
//
// Lanplus connections are not automatically closed. To close a connection the
// calling code must use the `CloseLanplus` API afterwards.
//
// The `open` interface is always connected and requires no keep alive.
class IpmiManager {
 public:
  // Options struct used to initialize the IpmiManager
  struct Options {
    // lanplus interfaces need to be poked periodically, this sets that interval
    absl::Duration lanplus_keep_aliver_interval = absl::Seconds(5);

    // The KeepAliver, which does the lanplus poking, needs to acquire a lock on
    // the IPMI arbiter. This option sets how long the KeepAliver thread should
    // wait before giving up.
    absl::Duration keepalive_arbiter_timeout = absl::Seconds(5);

    // This provides a place where a mocked clock can optionally be injected.
    Clock *clock = Clock::RealClock();

    // Provides the basis for open interfaces.
    std::function<ipmi_intf *()> open_factory;

    // Factory method providing the basis for lanplus interfaces.
    std::function<std::unique_ptr<ipmi_intf>()> lanplus_factory;
  };

  // RAII class that invokes Close with a given manager on destruction.
  class InterfaceCloser {
   public:
    InterfaceCloser(IpmiManager *manager, const IpmiInterfaceParams &params,
                    absl::Duration timeout);

    // Closes the specified interface.
    ~InterfaceCloser();

   private:
    // Manager to invoke CloseLanplus on.
    IpmiManager *manager_;

    // Params specifying the interface to close.
    IpmiInterfaceParams params_;

    // Time to wait to attempt to close the interface on destruction.
    absl::Duration timeout_;
  };

  // Convenience wrapper class of `InterfaceCloser` and `IpmiHandle`
  //
  // The class automatically releases handle and closes connection when it
  // goes out of scope.
  class HandleWithCloser {
   public:
    HandleWithCloser(std::unique_ptr<IpmiHandle> handle,
                     std::unique_ptr<InterfaceCloser> closer)
        : closer_(std::move(closer)), handle_(std::move(handle)) {}

    IpmiHandle *const handle() { return handle_.get(); }

   private:
    // The order of closer and handle is critical because destructors are
    // called in reverse order. The closer has to be the last to be destroyed,
    // otherwise the closer's destructor will not be able to acquire a lock
    // on the underlying interface.
    std::unique_ptr<InterfaceCloser> closer_;
    std::unique_ptr<IpmiHandle> handle_;
  };

  virtual ~IpmiManager() = default;

  // Use this when calling Acquire or Close, it will grab the global singleton
  // manager instance
  static IpmiManager &GlobalIpmiManager();

  // Helper functions for obtaining interface parameters.
  static IpmiInterfaceParams OpenParams();
  static IpmiInterfaceParams NetworkParams(absl::string_view hostname, int port,
                                           absl::string_view username,
                                           absl::string_view password);

  // Returns an IpmiHandle granting access to the requested IPMI interface.
  //
  // For "open" interfaces:
  // The open interface is more simple as compared to lanplus. The open
  // interface does not require a persistent connection. If `params` is not
  // initialized, the 'open' interface is assumed.
  //
  // For lanplus interfaces:
  // If the interface is not currently open, the manager will attempt to open
  // it. If successful that connection will remain open and kept alive until it
  // is explicitly closed. Must be closed explicitly in order to be correctly
  // cleaned up.
  //
  // Do not hold the IpmiHandle for any significant length of time as access to
  // IPMI is blocked so long as an IpmiHandle exists. Consider having the handle
  // as a promise that you have exclusive access to IPMI.
  virtual absl::StatusOr<std::unique_ptr<IpmiHandle>> Acquire(
      const IpmiInterfaceParams &params, absl::Duration timeout) = 0;

  // Closes a lanplus IPMI connection based on an lanplusInterfaceOptions.
  virtual absl::Status Close(const IpmiInterfaceParams &params,
                             absl::Duration timeout) = 0;

  // Returns an object that closes the given interface on destruction. The
  // manager must outlive the returned object.
  std::unique_ptr<InterfaceCloser> MakeCloser(
      const IpmiInterfaceParams &params,
      absl::Duration timeout = absl::InfiniteDuration());

  // Returns RAII `HandleWithCloser` wrapper with acquired handler and
  // closer
  absl::StatusOr<HandleWithCloser> GetHandleWithCloser(
      const IpmiInterfaceParams &params, absl::Duration handle_timeout,
      absl::Duration closer_timeout = absl::InfiniteDuration());

  // Enables logging for all IPMI traffic. Blocks indefinitely until all
  // prior-issued handles have been destroyed.
  virtual void EnableGlobalIpmiLogging() = 0;
};

class IpmiManagerImpl : public IpmiManager {
 public:
  explicit IpmiManagerImpl(const Options &options);

  absl::StatusOr<std::unique_ptr<IpmiHandle>> Acquire(
      const IpmiInterfaceParams &params, absl::Duration timeout) override;

  absl::Status Close(const IpmiInterfaceParams &params,
                     absl::Duration timeout) override;

  void EnableGlobalIpmiLogging() override;

  ~IpmiManagerImpl();

 private:
  // This method creates the KeepAliver thread. It does not start the thread.
  // This should only be called once, and only by the constructor of the
  // IpmiManager.
  void InitializeIpmiKeepaliver();

  // KeepAliver thread body, periodically itterates over the lanplus map
  // checking if the connections are still alive and running a keepalive process
  // on them. In the event a connection has been closed, an attempt to reconnect
  // is made.
  void RunKeepAliver();

  // Constructs a lanplus `ipmi_intf` from an `IpmiInterfaceParams::Network`
  std::unique_ptr<ipmi_intf> MakeLanplusInterface(
      const IpmiInterfaceParams::Network &lanplus_params);

  // This struct contains the configurable properties of the IPMI Manager
  const Options options_;

  // Map of lanplus interfaces via IpmiInterfaceParams. Any code touching this
  // should first attain a lock on the ipmi_arbiter prior to accessing.
  absl::flat_hash_map<std::string, std::unique_ptr<ipmi_intf>>
      lanplus_interfaces_;

  // IPMI arbiter, guards access to the lower-level IpmiTool lib
  //
  // ipmi_intf* is owned by the IpmiManager, specifically this pointer will
  // point to entries in either `lanplus_interfaces_` or the return value from
  // options_.open_interface().
  ecclesia::Arbiter<ipmi_intf *> ipmi_arbiter_;

  // Tracks if a keep alive thread is running, used to flag the thread to
  // terminate when necessary
  absl::Notification keepalive_terminate_notification_;

  // A reference to the keep alive thread
  std::unique_ptr<ecclesia::ThreadInterface> keepaliver_thread_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_IPMI_IPMI_MANAGER_H_
