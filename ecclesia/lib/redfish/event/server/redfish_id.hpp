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

// The redfish_id class implements the redfish id generation algorithm.
// The ids generated by the algorithm meet the following requirements:
// a) 64-bit.
// b) per process unique ids.
// c) monotonically increasing until INT64_MAX is reached at which point
// it resets to zero. In reality, the frequency of gBMC deployment and restart
// should prevent overflow from occurring.
//
// The generated ids can start at a user specified value, which is useful
// if we want to resume id generation. Note that the ids generated by this
// algorithm are not globally unique ids.
// These ids are good for subscription id, event id, etc.
// For event id, as long as events don't persist across restarts, any duplicates
// in ids across process restarts is a non-issue.

#ifndef ECCLESIA_LIB_REDFISH_EVENT_SERVER_ID_H_
#define ECCLESIA_LIB_REDFISH_EVENT_SERVER_ID_H_

#include <cstdint>
#include <stdlib.h>
#include <time.h>

// The magic number for the start id.
// When the magic number is specified, start id is generated randomly.
// The alternate is a user specified start id.
constexpr int64_t START_ID_MAGIC_NUMBER = 16011979L;

namespace ecclesia {

class RedfishIdNonlock
{
public:
    void Lock()
    {
    }
    void Unlock()
    {
    }
};

// The redfish_id class has one template parameter:
// Lock is a type that supports Lock() and Unlock() interfaces
// (such as absl::Mutex).
// The default Lock type is RedfishIdNonlock that has noop for Lock()
// and Unlock(). By default, a RedfishId object is not thread-compatible.
// To share an object of RedfishId across threads, use absl::Mutex Lock type.
template<typename Lock = RedfishIdNonlock>
class RedfishId
{
    using lock_type = Lock;
    int64_t last_id_;
    lock_type lock_;
public:
    RedfishId(): last_id_(START_ID_MAGIC_NUMBER) {}
    explicit RedfishId(int64_t user_id): last_id_(user_id) {}

    RedfishId(RedfishId&&) = delete;
    RedfishId& operator=(RedfishId&&) = delete;
    RedfishId(const RedfishId&) = delete;

    RedfishId& operator=(const RedfishId&) = delete;

    int64_t NextId()
    {
        lock_.Lock();
        if (last_id_ == START_ID_MAGIC_NUMBER) {
          srand((unsigned) time(NULL));
          last_id_ = rand() % 100000000;
        } else if (last_id_ + 1 >= INT64_MAX) {
          last_id_ = 0;
        } else {
          last_id_++;
        }
        lock_.Unlock();
        return last_id_;
    }
};

}  // namespace ecclesia

#endif