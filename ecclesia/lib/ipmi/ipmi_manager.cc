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

#include "ecclesia/lib/ipmi/ipmi_manager.h"

#include <sys/socket.h>
#include <syslog.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "ecclesia/lib/arbiter/arbiter.h"
#include "ecclesia/lib/ipmi/ipmi_consts.h"
#include "ecclesia/lib/ipmi/ipmi_handle.h"
#include "ecclesia/lib/ipmi/ipmi_interface_params.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "ecclesia/lib/thread/thread.h"
#include "ecclesia/lib/time/clock.h"

extern "C" {
#include "include/ipmitool/ipmi_intf.h"
#include "include/ipmitool/ipmi_sol.h"  // IWYU pragma: keep
#include "include/ipmitool/log.h"

// This struct is defined in src/plugins/open/open.c

extern struct ipmi_intf ipmi_lanplus_intf;
}

ABSL_FLAG(absl::Duration, ipmi_lanplus_keep_aliver_interval, absl::Seconds(5),
          "The amount of time to wait between keep alive runs");

ABSL_FLAG(absl::Duration, ipmi_connection_timeout, absl::Seconds(5),
          "The amount of time to wait for IPMI connections to timeout");

ABSL_FLAG(bool, ipmi_enable_global_logging, false,
          "Should IPMI manager initialize with global logging enabled");

namespace ecclesia {

namespace {

// Shorthand function for determining whether `params` refers to the "open"
// interface.
bool IsOpenInterface(const IpmiInterfaceParams &params) {
  switch (params.params_case()) {
    case IpmiInterfaceParams::PARAMS_NOT_SET:
    case IpmiInterfaceParams::kOpen:
      return true;
    case IpmiInterfaceParams::kNetwork:
      return false;
  }
  LOG(ERROR)
      << "Processed IpmiInterfaceParams in `IsOpenInterface` which had invalid "
         "params, assuming open params. Params: "
      << params.DebugString();
  return true;
}

// Utility to convert the IpmiInterfaceParams struct to a string
std::string OptsToString(const IpmiInterfaceParams &params) {
  if (IsOpenInterface(params)) {
    return "open";
  }
  const IpmiInterfaceParams::Network &lanplus_params = params.network();
  return absl::StrCat(lanplus_params.username(), "@", lanplus_params.hostname(),
                      ":", lanplus_params.port());
}

ipmi_intf *open_factory() { return &ipmi_open_intf; }

std::unique_ptr<ipmi_intf> lanplus_factory() {
  return std::make_unique<ipmi_intf>(ipmi_lanplus_intf);
}

IpmiManager::Options OptionsFromFlags() {
  return IpmiManager::Options{
      .lanplus_keep_aliver_interval =
          absl::GetFlag(FLAGS_ipmi_lanplus_keep_aliver_interval),
      .keepalive_arbiter_timeout = absl::GetFlag(FLAGS_ipmi_connection_timeout),
      .open_factory = open_factory,
      .lanplus_factory = lanplus_factory};
}

std::unique_ptr<IpmiManagerImpl> MakeGlobalManager() {
  auto manager = std::make_unique<IpmiManagerImpl>(OptionsFromFlags());

  if (absl::GetFlag(FLAGS_ipmi_enable_global_logging)) {
    manager->EnableGlobalIpmiLogging();
  }

  return manager;
}

}  // namespace

IpmiInterfaceParams IpmiManager::OpenParams() {
  IpmiInterfaceParams params;
  params.mutable_open();
  return params;
}

IpmiInterfaceParams IpmiManager::NetworkParams(absl::string_view hostname,
                                               int port,
                                               absl::string_view username,
                                               absl::string_view password) {
  IpmiInterfaceParams params;
  IpmiInterfaceParams::Network *network_params = params.mutable_network();

  network_params->set_hostname(std::string(hostname));
  network_params->set_port(port);
  network_params->set_username(std::string(username));
  network_params->set_password(std::string(password));

  return params;
}

IpmiManager::InterfaceCloser::InterfaceCloser(IpmiManager *manager,
                                              const IpmiInterfaceParams &params,
                                              absl::Duration timeout)
    : manager_(manager), params_(params), timeout_(timeout) {}

IpmiManager::InterfaceCloser::~InterfaceCloser() {
  absl::Status status = manager_->Close(params_, timeout_);
  if (!status.ok()) {
    LOG(ERROR) << "Could not close interface " << OptsToString(params_) << ": "
               << status;
  }
}

std::unique_ptr<IpmiManager::InterfaceCloser> IpmiManager::MakeCloser(
    const IpmiInterfaceParams &params, absl::Duration timeout) {
  return std::make_unique<InterfaceCloser>(this, params, timeout);
}

absl::StatusOr<IpmiManager::HandleWithCloser> IpmiManager::GetHandleWithCloser(
    const IpmiInterfaceParams &params, absl::Duration handle_timeout,
    absl::Duration closer_timeout) {
  ECCLESIA_ASSIGN_OR_RETURN(std::unique_ptr<IpmiHandle> handle,
                            Acquire(params, handle_timeout));
  return IpmiManager::HandleWithCloser{std::move(handle),
                                       MakeCloser(params, closer_timeout)};
}

IpmiManagerImpl::IpmiManagerImpl(const Options &options)
    : options_(options),
      ipmi_arbiter_(
          Arbiter<ipmi_intf *>(std::make_unique<ipmi_intf *>(nullptr))),
      keepaliver_thread_(GetDefaultThreadFactory()->New(
          absl::bind_front(&IpmiManagerImpl::RunKeepAliver, this))) {}

absl::StatusOr<std::unique_ptr<IpmiHandle>> IpmiManagerImpl::Acquire(
    const IpmiInterfaceParams &params, absl::Duration timeout) {
  // Get a lock on the mutex, do this early to fully lock IPMI access while
  // we are manipulating the map
  ECCLESIA_ASSIGN_OR_RETURN(ExclusiveLock<ipmi_intf *> exclusive_lock,
                            ipmi_arbiter_.Acquire(timeout));

  // The "open" interface is treated specially as compared to lanplus intfs
  if (IsOpenInterface(params)) {
    *exclusive_lock = options_.open_factory();
    return std::make_unique<IpmiHandleImpl>(std::move(exclusive_lock));
  }

  const std::string key = OptsToString(params);

  // Acquire is expected to create interfaces that do not exist in the map,
  // the next section of code does this.

  auto it = lanplus_interfaces_.find(key);

  // If the interface doesn't exist in the map, create and open
  if (it == lanplus_interfaces_.end()) {
    std::unique_ptr<ipmi_intf> new_interface =
        MakeLanplusInterface(params.network());

    if (ipmi_open(new_interface.get()) < 0) {
      ipmi_cleanup(new_interface.get());
      ipmi_close(new_interface.get());
      return absl::InternalError("Failed to open IPMI interface " + key);
    }

    it = lanplus_interfaces_.insert({key, std::move(new_interface)}).first;

    LOG(INFO) << "Successfully opened IPMI interface " << key;
  }

  // Either the iterator did not return `end()`, or the preceding block
  // created a new element and set the iterator to that element.
  std::unique_ptr<ipmi_intf> &interface = it->second;

  // Set the active interface
  *exclusive_lock = interface.get();

  return std::make_unique<IpmiHandleImpl>(std::move(exclusive_lock));
}

absl::Status IpmiManagerImpl::Close(const IpmiInterfaceParams &params,
                                    absl::Duration timeout) {
  // Get a lock on the mutex, do this early to fully lock IPMI access while
  // we are manipulating the map
  ECCLESIA_ASSIGN_OR_RETURN(ExclusiveLock<ipmi_intf *> exclusive_lock,
                            ipmi_arbiter_.Acquire(timeout));

  if (IsOpenInterface(params)) {
    ipmi_cleanup(options_.open_factory());
    ipmi_close(options_.open_factory());
    return absl::OkStatus();
  }

  const std::string key = OptsToString(params);

  auto it = lanplus_interfaces_.find(key);

  if (it == lanplus_interfaces_.end()) {
    return absl::InternalError("Attempt to close interface that is not open: " +
                               key);
  }

  ipmi_cleanup(it->second.get());
  ipmi_close(it->second.get());

  lanplus_interfaces_.erase(key);

  return absl::OkStatus();
}

void IpmiManagerImpl::EnableGlobalIpmiLogging() {
  ExclusiveLock<ipmi_intf *> lock = ipmi_arbiter_.AcquireOrDie();

  LOG(INFO) << "Enabling global verbose logging";
  log_halt();
  log_init(/*name=*/nullptr, /*isdaemon=*/0, /*verbose=*/2);
  lprintf(LOG_DEBUG, "ipmitool logging enabled");
}

IpmiManagerImpl::~IpmiManagerImpl() {
  // Force keepaliver thread to safely exit
  keepalive_terminate_notification_.Notify();
  keepaliver_thread_->Join();
}

void IpmiManagerImpl::RunKeepAliver() {
  DLOG(INFO) << "Running keepalive on lanplus interfaces...";

  while (true) {
    // Wait the specified amount of time between keep alive attempts
    options_.clock->Sleep(options_.lanplus_keep_aliver_interval);

    // Terminate if keepalive_ flag is ever unset
    if (keepalive_terminate_notification_.HasBeenNotified()) {
      break;
    }

    // This is strange but we need to acquire the handle *just* to get a
    // lock on the IPMI, we will not actually be using it as the keep
    // aliver can (and needs to) directly access the raw_intf from the
    // lan_plus map
    absl::StatusOr<ExclusiveLock<ipmi_intf *>> handle =
        ipmi_arbiter_.Acquire(options_.keepalive_arbiter_timeout);

    if (!handle.ok()) {
      LOG(ERROR) << "Failed to acquire handle: " << handle.status()
                 << ", with timeout: " << options_.keepalive_arbiter_timeout;
      return;
    }

    for (const auto &[connection_name, interface] : lanplus_interfaces_) {
      DLOG(INFO) << "Keepaliving " << connection_name;

      // Verify the map entry contains a value, it should never be allowed
      // to have empty values, but we still want to check here
      CHECK(interface != nullptr)
          << "Interface cannot be nullptr"
          << "lanplus_interfaces_ map value empty for key " << connection_name;

      if (interface->keepalive == nullptr) {
        continue;
      }

      int res = interface->keepalive(interface.get());
      if (res != 0) {
        LOG(ERROR) << "Error while sending keepalive: " << res;
        ipmi_close(interface.get());

        if (ipmi_open(interface.get()) >= 0) {
          LOG(WARNING) << "Re-opened ipmi raw_intf for connection "
                       << connection_name;
        } else {
          LOG(ERROR) << "Fail to re-open ipmi raw_intf for connection "
                     << connection_name;
        }
      }
    }
  }
}

// This creates a lan plus ipmi_intf struct, used when a call to
// Acquire has params that are not currently in the map, causing a new
// interface to have to be opened in Open and added to the map
//
// The "open" interface is not created in this fashion, instead the extern
// global from the IPMI tool itself is used. The "open" interface is also
// not added to the map, as it is treated as a special singleton
//
// Should only be called by methods holding an ExclusiveLock
// from the ipmi_arbiter_
std::unique_ptr<ipmi_intf> IpmiManagerImpl::MakeLanplusInterface(
    const IpmiInterfaceParams::Network &lanplus_params) {
  // Obtain a base interface from the lanplus factory method passed in via the
  // `options_` struct.
  std::unique_ptr<ipmi_intf> interface = options_.lanplus_factory();

  // Generate the hostname
  std::vector<char> hostname(
      lanplus_params.hostname().c_str(),
      lanplus_params.hostname().c_str() + lanplus_params.hostname().size() + 1);
  ipmi_intf_session_set_hostname(interface.get(), hostname.data());

  // Generate the username
  std::vector<char> username(
      lanplus_params.username().c_str(),
      lanplus_params.username().c_str() + lanplus_params.username().size() + 1);
  ipmi_intf_session_set_username(interface.get(), username.data());

  // Generate the password
  std::vector<char> password(
      lanplus_params.password().c_str(),
      lanplus_params.password().c_str() + lanplus_params.password().size() + 1);
  ipmi_intf_session_set_password(interface.get(), password.data());

  ipmi_intf_session_set_port(interface.get(), lanplus_params.port());

  uint8_t kgkey[IPMI_KG_BUFFER_SIZE] = {0};
  ipmi_intf_session_set_kgkey(interface.get(), kgkey);
  ipmi_intf_session_set_privlvl(interface.get(), IPMI_SESSION_PRIV_ADMIN);
  ipmi_intf_session_set_lookupbit(interface.get(), kIpmiDefaultLookupBit);
  ipmi_intf_session_set_sol_escape_char(interface.get(),
                                        SOL_ESCAPE_CHARACTER_DEFAULT);
  ipmi_intf_session_set_cipher_suite_id(interface.get(),
                                        IPMI_LANPLUS_CIPHER_SUITE_3);
  interface->devnum = 0;
  interface->devfile = nullptr;
  interface->ai_family = AF_UNSPEC;
  interface->my_addr = IPMI_BMC_SLAVE_ADDR;
  interface->target_addr = IPMI_BMC_SLAVE_ADDR;

  return interface;
}

IpmiManager &IpmiManager::GlobalIpmiManager() {
  // Magic static pattern
  static IpmiManager *manager = MakeGlobalManager().release();
  return *manager;
}

}  // namespace ecclesia
