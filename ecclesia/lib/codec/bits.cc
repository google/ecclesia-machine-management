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

#include "ecclesia/lib/codec/bits.h"

#include <algorithm>
#include <ostream>

namespace ecclesia {

MaskedAddress::iterator &MaskedAddress::iterator::operator++() {
  // cycle through matching addresses by forcing carries to propagate
  // through bits which have the corresponding bit in the mask set,
  // then restoring those bits.
  addr_ = (((addr_ | mask_) + 1) & ~mask_) | (addr_ & mask_);
  if ((addr_ & ~mask_) == 0) {
    done_ = true;
  }
  return *this;
}

MaskedAddress::iterator MaskedAddress::iterator::operator++(int) {
  iterator it(*this);
  ++*this;
  return it;
}

bool MaskedAddress::MaskedAddress::iterator::operator==(
    const iterator &i) const {
  // All finished iterators compare equal.
  if (done_ && i.done_) {
    return true;
  }
  // Finished and unfinished iterators compare not equal.
  if (done_ || i.done_) {
    return false;
  }
  // Otherwise compare the actual address and mask.
  return addr_ == i.addr_ && mask_ == i.mask_;
}

std::ostream &operator<<(std::ostream &out, const MaskedAddress &ma) {
  out << ma.addr_ << "\\" << ma.mask_;
  return out;
}

bool AddressRange::Empty() const { return end_ <= begin_; }

bool AddressRange::InRange(uint64_t address) const {
  if (Empty()) {
    return false;
  }
  return address >= begin_ && address <= end_;
}

bool AddressRange::CoversMask(MaskedAddress address) const {
  return address.first() >= begin_ && address.last() <= end_;
}

bool AddressRange::OverlapsMask(MaskedAddress address) const {
  if (end_ < begin_) {
    return false;
  }
  if (CoversMask(address)) {
    return true;
  }
  if (address.Contiguous()) {
    // `address` straddles beginning of range.
    if (address.first() <= begin_ && address.last() > begin_) {
      return true;
    }
    // `address` straddles end of range.
    if (address.first() < end_ && address.last() >= end_) {
      return true;
    }
  }
  if (address.last() < begin_ || address.first() > end_) {
    return false;
  }
  // inefficient case
  return std::any_of(address.begin(), address.end(),
                     [this](uint64_t i) { return begin_ <= i && i <= end_; });
}

std::ostream &operator<<(std::ostream &out, const AddressRange &addr) {
  out << "[" << addr.begin_ << ":" << addr.end_ << "]";
  return out;
}

}  // namespace ecclesia
