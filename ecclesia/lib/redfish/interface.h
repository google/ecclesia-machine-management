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
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "ecclesia/lib/http/codes.h"

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
//
// It is possible to access a nested Redfish structure by chaining the index
// operator. For example `root[kRfPropertySystems][1]` gives the 2nd
// ComputerSystem object in the Systems node as a RedfishVariant. In the case of
// having a RedfishIterable in the chain, it is also possible to loop through
// the iterator with `.Each()`. In that case a `.Do()` call is needed at the end
// of the chain to specify the procedure to be applied on each object. If any
// segment in the chain is null, the final result is null.
//
// These subscriptions and functions can appear multiple times in the chain. For
// example,
//
//     RedfishVariant::IndexEach each;
//     root[kRfPropertySystems].Each()[kRfPropertyMemory].Each().Do(
//       [](auto mem_obj) {...});
//
// this applies the closure on each of the memory nodes in each system.
//
// RedfishVariant contains an absl::Status member. A non-OK absl::Status may
// still contain a valid Redfish payload (e.g. a POST operation failed and the
// detailed structured JSON error is in the payload). One should treat the state
// of the absl::Status member and the Redfish payload as independent. The
// Redfish specification can provide clarity on the situations in which one
// could expect errors and/or payloads.
//
// RedfishVariant contains a httpcode member. This field is only populated if a
// HTTP request was required to produce the RedfishVariant. Some operations
// which construct a RedfishVariant may not involve HTTP requests (e.g. drilling
// down into child fields of a payload).
class RedfishVariant final {
 public:
  // ImplIntf is provided as the interface for subclasses to be implemented with
  // the PImpl idiom.
  class ImplIntf {
   public:
    virtual ~ImplIntf() {}
    virtual std::unique_ptr<RedfishObject> AsObject() const = 0;
    virtual std::unique_ptr<RedfishIterable> AsIterable() const = 0;
    virtual bool GetValue(std::string *val) const = 0;
    virtual bool GetValue(int32_t *val) const = 0;
    virtual bool GetValue(int64_t *val) const = 0;
    virtual bool GetValue(double *val) const = 0;
    virtual bool GetValue(bool *val) const = 0;
    virtual bool GetValue(absl::Time *val) const = 0;
    virtual std::string DebugString() const = 0;
  };

  // A helper class to denote a loop through an iterator.
  class IndexEach {};
  using IndexType = absl::variant<std::string, size_t, IndexEach>;

  // A helper class for lazy evaluation of an index operator chain, when
  // IndexEach is involved.
  class IndexHelper {
   public:
    IndexHelper() = delete;
    IndexHelper(const RedfishVariant &root, IndexType index) : root_(root) {
      AppendIndex(index);
    }

    void AppendIndex(const IndexType &index) { indices_.push_back(index); }

    bool IsEmpty() const { return indices_.empty(); }

    IndexHelper Each() {
      AppendIndex(IndexEach());
      return *this;
    }

    IndexHelper &operator[](const IndexType &index) {
      AppendIndex(index);
      return *this;
    }

    // This should be called at the end of the chain. For each “leaf” node that
    // the chain finds, the functional `what` will be called with the node
    // passed to it as the sole argument. The node is passed as a unique_ptr to
    // the RedfishObject.
    template <typename F>
    void Do(F what) const {
      Do(root_, indices_, what);
    }

   private:
    // template <typename F>
    // void Do(const RedfishVariant &root, absl::Span<const IndexType> indices,
    //         F what) const;
    template <typename F>
    void Do(const RedfishVariant &root, absl::Span<const IndexType> indices,
            F what) const;

    std::vector<IndexType> indices_;
    const RedfishVariant &root_;
  };

  RedfishVariant() : ptr_(nullptr) {}

  // Construct from Status.
  explicit RedfishVariant(absl::Status status)
      : ptr_(nullptr), status_(std::move(status)) {}

  // Construct from ptr: the Redfish data is valid and there were no errors.
  explicit RedfishVariant(std::unique_ptr<ImplIntf> ptr)
      : ptr_(std::move(ptr)), status_(absl::OkStatus()) {}

  // Construct from httpcode + error object. A Status is constructed by
  // converting the provided httpcode.
  RedfishVariant(std::unique_ptr<ImplIntf> ptr,
                 ecclesia::HttpResponseCode httpcode)
      : ptr_(std::move(ptr)),
        status_(ecclesia::HttpResponseCodeToCanonical(httpcode),
                ecclesia::HttpResponseCodeToReasonPhrase(httpcode)),
        httpcode_(httpcode) {}

  RedfishVariant(const RedfishVariant &) = delete;
  RedfishVariant &operator=(const RedfishVariant &) = delete;
  RedfishVariant(RedfishVariant &&other) = default;
  RedfishVariant &operator=(RedfishVariant &&other) = default;

  inline RedfishVariant operator[](const std::string &property) const;
  inline RedfishVariant operator[](const size_t index) const;
  IndexHelper Each() const {
    return IndexHelper(*this, IndexType(IndexEach()));
  }

  std::unique_ptr<RedfishObject> AsObject() const {
    if (!ptr_) return nullptr;
    return ptr_->AsObject();
  }
  std::unique_ptr<RedfishIterable> AsIterable() const {
    if (!ptr_) return nullptr;
    return ptr_->AsIterable();
  }

  // Returns the status of the RedfishVariant.
  // Note that the status is independent from the Redfish Payload. See the
  // class-level docstring for more information.
  const absl::Status &status() const { return status_; }

  // Returns the httpcode, if one is available. See the class-level docstring
  // for more information.
  const absl::optional<ecclesia::HttpResponseCode> &httpcode() const {
    return httpcode_;
  }

  // If the underlying Redfish payload is the provided val type, retrieves the
  // value into val and return true. Otherwise return false.
  bool GetValue(std::string *val) const {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(int32_t *val) const {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(int64_t *val) const {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(double *val) const {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(bool *val) const {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }
  bool GetValue(absl::Time *val) const {
    if (!ptr_) return false;
    return ptr_->GetValue(val);
  }

  std::string DebugString() const {
    if (!ptr_) return "";
    return ptr_->DebugString();
  }

 private:
  std::unique_ptr<ImplIntf> ptr_;
  absl::Status status_;
  absl::optional<ecclesia::HttpResponseCode> httpcode_;
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
  virtual RedfishVariant operator[](int index) const = 0;

  class Iterator {
   public:
    using difference_type = size_t;
    using value_type = RedfishVariant;
    using pointer = void;
    using reference = RedfishVariant;
    using iterator_category = std::input_iterator_tag;

    reference operator*() {
      size_t size = iterable_->Size();
      if (index_ < size) {
        return (*iterable_)[index_];
      }
      return RedfishVariant(absl::OutOfRangeError(absl::StrFormat(
          "index %zu is out of bounds (iterable has size %zu)", index_, size)));
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
  virtual RedfishVariant operator[](const std::string &node_name) const = 0;
  virtual absl::optional<std::string> GetUri() = 0;

  virtual std::string DebugString() = 0;

  // GetNodeValue is a convenience method which calls GetNode() then GetValue().
  // If the node does not exist or if the value could not be retrieved, nullopt
  // will be returned. Returns typed value or nullopt on error.
  template <typename T>
  absl::optional<T> GetNodeValue(absl::string_view node_name) const {
    T val;
    auto node = (*this)[node_name.data()];
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
  virtual RedfishVariant PostUri(absl::string_view uri,
                                 absl::string_view data) = 0;

  // Patch to the given URI and returns result.
  virtual RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) = 0;

  // Patch to the given URI and returns result.
  virtual RedfishVariant PatchUri(absl::string_view uri,
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
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant PatchUri(absl::string_view uri,
                          absl::string_view data) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
};

RedfishVariant RedfishVariant::operator[](const std::string &property) const {
  if (!ptr_) return RedfishVariant();
  if (std::unique_ptr<RedfishObject> obj = AsObject()) {
    return (*obj)[property];
  } else {
    return RedfishVariant(absl::InternalError("not a RedfishObject"));
  }
}
RedfishVariant RedfishVariant::operator[](const size_t index) const {
  if (!ptr_) return RedfishVariant();
  if (std::unique_ptr<RedfishIterable> iter = AsIterable()) {
    return (*iter)[index];
  } else {
    return RedfishVariant(absl::InternalError("not a RedfishIterable"));
  }
}

// Evaluates the index chain in a recursive fashion.
template <typename F>
void RedfishVariant::IndexHelper::Do(const RedfishVariant &root,
                                     absl::Span<const IndexType> indices,
                                     F what) const {
  if (indices.empty()) {
    // The chain is empty. That means we have evaluated the whole chain. Variant
    // `root` should be one of the objects the chain matches. So we will just
    // call `what` on it, and stop the recursion.
    if (std::unique_ptr<RedfishObject> obj = root.AsObject()) {
      what(obj);
    }
    return;
  }

  // The chain is not empty. We will evaluate the 1st index in the chain, and
  // leave the rest to the next layer of recursion.
  const IndexType &index = indices[0];
  absl::visit(
      [&](auto &&index_value) {
        using T = std::decay_t<decltype(index_value)>;
        auto rest = indices.last(indices.size() - 1);
        if constexpr (std::is_same_v<T, std::string>) {
          // The index is a string. Likely we are looking at a RedfishObject.
          // Simply drill down.
          Do(root[index_value], rest, what);
        } else if constexpr (std::is_same_v<T, size_t>) {
          // The index is an integer. We should be looking at a RedfishIterable.
          // Simply drill down.
          Do(root[index_value], rest, what);
        } else if constexpr (std::is_same_v<T, IndexEach>) {
          // This segment of the chain is an `Each()`. Therefore we are looking
          // at a RedfishIterable, and need to iterate over its elements. We
          // then drill down to each of the elements.
          auto iter = root.AsIterable();
          if (!iter) return;
          for (auto entry : *iter) {
            Do(entry, rest, what);
          }
        }
      },
      index);
}

}  // namespace libredfish

#endif  // ECCLESIA_LIB_REDFISH_INTERFACE_H_
