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

#ifndef ECCLESIA_LIB_REDFISH_INTERFACE_H_
#define ECCLESIA_LIB_REDFISH_INTERFACE_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"

namespace libredfish {

// Forward declare the typed view classes
class RedfishIterable;
class RedfishObject;

// RedfishVariant is the standard return type for all Redfish interfaces.
// Its purpose is to force the caller to strictly specify the expected Redfish
// view to access the underlying Redfish payload internals.
//
// RedfishVariant must not be the sole owner of the the Redfish data. Views
// created from RedfishVariant are permitted to outlive the RedfishVariant.
//
// If the given Redfish getter fails or there is a type mismatch, a null view
// will be returned by the View methods.
class RedfishVariant final {
 public:
  // ImplIntf is provided as the interface for subclasses to be implemented with
  // the PImpl idiom.
  class ImplIntf {
   public:
    virtual ~ImplIntf() {}
    virtual std::unique_ptr<RedfishObject> AsObject() = 0;
    virtual std::unique_ptr<RedfishIterable> AsIterable() = 0;
    virtual bool GetValue(std::string *val) = 0;
    virtual bool GetValue(int32_t *val) = 0;
    virtual bool GetValue(int64_t *val) = 0;
    virtual bool GetValue(double *val) = 0;
    virtual bool GetValue(bool *val) = 0;
    virtual std::string DebugString() = 0;
  };

  RedfishVariant() : ptr_(nullptr) {}
  RedfishVariant(std::unique_ptr<ImplIntf> ptr) : ptr_(std::move(ptr)) {}
  RedfishVariant(const RedfishVariant &) = delete;
  RedfishVariant &operator=(const RedfishVariant &) = delete;
  RedfishVariant(RedfishVariant &&other) = default;
  RedfishVariant &operator=(RedfishVariant &&other) = default;

  std::unique_ptr<RedfishObject> AsObject() {
    if (!ptr_) return nullptr;
    return ptr_->AsObject();
  }
  std::unique_ptr<RedfishIterable> AsIterable() {
    if (!ptr_) return nullptr;
    return ptr_->AsIterable();
  }

  // If the underlying Redfish payload is the provided val type, retrieves the
  // value into val and return true. Otherwise return false.
  bool GetValue(std::string *val) {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(int32_t *val) {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(int64_t *val) {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(double *val) {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(bool *val) {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }

  std::string DebugString() { return ptr_->DebugString(); }

 private:
  std::unique_ptr<ImplIntf> ptr_;
};

// RedfishIterable provides an interface for accessing properties of either
// a Redfish Collection or a Redfish Array.
class RedfishIterable {
 public:
  RedfishIterable() {}
  virtual ~RedfishIterable() {}
  // Returns the number of elements in the Collection or Array.
  virtual size_t Size() = 0;
  // Returns true if the Collection or Array contains 0 elements.
  virtual bool Empty() = 0;
  // Returns the payload for a given index. If the value of the node is an
  // "@odata.id" field, the RedfishInterface will be queried to retrieve the
  // payload corresponding to that "@odata.id".
  virtual RedfishVariant GetIndex(int index) = 0;

  class Iterator {
   public:
    using difference_type = size_t;
    using value_type = RedfishVariant;
    using pointer = void;
    using reference = RedfishVariant;
    using iterator_category = std::input_iterator_tag;

    reference operator*() {
      if (index_ < iterable_->Size()) {
        return iterable_->GetIndex(index_);
      }
      return RedfishVariant();
    }

    Iterator &operator++() {
      ++index_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator temp = *this;
      ++index_;
      return temp;
    }

    bool operator==(const Iterator &other) const {
      return std::tie(iterable_, index_) ==
             std::tie(other.iterable_, other.index_);
    }
    bool operator!=(const Iterator &other) const { return !operator==(other); }

   private:
    Iterator(RedfishIterable *iterable, size_t index)
        : iterable_(iterable), index_(index) {}
    friend class RedfishIterable;

    RedfishIterable *iterable_;
    size_t index_;
  };
  Iterator begin() { return Iterator(this, 0); }
  Iterator end() { return Iterator(this, Size()); }
};

// RedfishObject provides an interafce for accessing properties of Redfish
// Objects.
class RedfishObject {
 public:
  RedfishObject() {}
  virtual ~RedfishObject() {}
  // Returns the payload for a given named property node. If the value of
  // the node is an "@odata.id" field, the RedfishInterface will be queried
  // to retrieve the payload corresponding to that "@odata.id".
  virtual RedfishVariant GetNode(const std::string &node_name) const = 0;
  virtual absl::optional<std::string> GetUri() = 0;

  // GetNodeValue is a convenience method which calls GetNode() then GetValue().
  // If the node does not exist or if the value could not be retrieved, nullopt
  // will be returned. Returns typed value or nullopt on error.
  template <typename T>
  absl::optional<T> GetNodeValue(absl::string_view node_name) const {
    T val;
    auto node = GetNode(node_name.data());
    if (!node.GetValue(&val)) return absl::nullopt;
    return val;
  }
  template <typename PropertyDefinitionT>
  absl::optional<typename PropertyDefinitionT::type> GetNodeValue() const {
    return GetNodeValue<typename PropertyDefinitionT::type>(
        PropertyDefinitionT::Name);
  }
};

// RedfishInterface provides initial access points to the Redfish resource tree.
class RedfishInterface {
 public:
  using ValueVariant =
      absl::variant<int, bool, std::string, const char *, double>;

  virtual ~RedfishInterface() {}

  // An endpoint is trusted if all of the information coming from the endpoint
  // can be reliably assumed to be from a Google-controlled source.
  // Examples of trusted endpoints are attested BMCs and prodimage running in
  // caretaker mode.
  // Examples of nontrusted endpoints are Redfish services running on a
  // baremetal customer's machine, and attested agents serving information
  // about hardware that has been exposed via passthrough to a VM customer
  // (where a customer can tamper with the hardware's persistent state to
  // misreport information).
  enum TrustedEndpoint { kTrusted, kUntrusted };

  // Updates the endpoint for sending Redfish requests.
  virtual void UpdateEndpoint(absl::string_view endpoint,
                              TrustedEndpoint trusted) = 0;

  // Returns whether the endpoint is trusted.
  virtual bool IsTrusted() const = 0;

  // Fetches the root payload and returns it.
  virtual RedfishVariant GetRoot() = 0;

  // Fetches the given URI and returns it.
  virtual RedfishVariant GetUri(absl::string_view uri) = 0;

  // Post to the given URI and returns result.
  virtual RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) = 0;

  // Post to the given URI and returns result.
  virtual RedfishVariant PostUri(
      absl::string_view uri,
      absl::string_view data) = 0;
};

// Concrete implementation to provide a null placeholder interface which returns
// no data on all requests.
class NullRedfish : public RedfishInterface {
  // The endpoint cannot be updated, even to a valid endpoint. Note that this
  // assumes that you've ended up with a NullRedfish instance for reasons other
  // than a more concrete interface failing to connect to a provided endpoint.
  void UpdateEndpoint(absl::string_view endpoint,
                      TrustedEndpoint trusted) override {}
  // The null endpoint is trusted as it doesn't provide any system information,
  // so there is nothing it could lie about.
  bool IsTrusted() const override { return true; }
  RedfishVariant GetRoot() override { return RedfishVariant(); }
  RedfishVariant GetUri(absl::string_view uri) override {
    return RedfishVariant();
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant();
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::string_view data) override {
    return RedfishVariant();
  }
};

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_INTERFACE_H_
