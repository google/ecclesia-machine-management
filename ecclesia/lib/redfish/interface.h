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
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "ecclesia/lib/http/codes.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/interface.h"
#include "single_include/nlohmann/json.hpp"

namespace ecclesia {

enum class ServiceRootUri { kRedfish, kGoogle };

// Forward declare the typed view classes
class RedfishIterable;
class RedfishObject;

// Return value for iterator functions passed to Redfish interface functions.
enum class RedfishIterReturnValue {
  // Continue searching for additional resources and invoking the callback.
  kContinue = 0,
  // Stop searching for resources. The callback will not be invoked again.
  kStop
};

// Stores what RedFish features are supported by the specific backend.
// See ProtocolFeaturesSupported defenition:
//    https://www.dmtf.org/sites/default/files/standards/documents/DSP0268_2021.4_0.pdf
struct RedfishSupportedFeatures {
  struct Expand {
    // This property shall indicate whether this service supports the asterisk
    // option of the $expand query parameter. Meaning of (*):
    // (*): Shall expand all hyperlinks, including those in payload
    // annotations, such as @Redfish.Settings, @Redfish.ActionInfo, and
    // @Redfish.CollectionCapabilities.
    bool expand_all = false;
    // This property shall indicate whether the service supports the $levels
    // option of the $expand query parameter.
    bool levels = false;
    // This property shall indicate whether this service supports the tilde (~)
    // option of the $expand query parameter. Meaning of (~):
    // (~) : Shall expand all hyperlinks found in all links property - instances
    // of the resource.
    bool links = false;
    // This property shall indicate whether the service supports the period (.)
    // option of the $expand query parameter. Meaning of (.):
    // - Shall expand all hyperlinks not in any links property instances of the
    // resource, including those in payload annotations, such as
    // @Redfish.Settings, @Redfish.ActionInfo, and
    // @Redfish.CollectionCapabilities .
    bool no_links = false;
    // This property shall contain the maximum $levels option value in the
    // $expand query parameter. Shall be included only if $levels is true.
    int max_levels = 0;
  };
  Expand expand;
};

// Classes below provide an interface to supply query parameters to mmanager
// redfish clients.
class GetParamQueryInterface {
 public:
  virtual ~GetParamQueryInterface() = default;
  // Method to convert query parameter to the part of URI
  virtual std::string ToString() const = 0;
};

// Defines Expand parameter
// See RedFish spec 7.3.1. Use of the $expand query parameter
class RedfishQueryParamExpand : public GetParamQueryInterface {
 public:
  enum ExpandType { kBoth, kNotLinks, kLinks };

  struct Params {
    ExpandType type = ExpandType::kNotLinks;
    size_t levels = 1;
  };

  RedfishQueryParamExpand(Params params);

  // Validates if redfish agent supports the Expand query args.
  // Redfish agent is supposed to return ProtocolFeaturesSupported as part of
  // the /redfish/v1 object
  absl::Status ValidateRedfishSupport(
      const absl::optional<RedfishSupportedFeatures> &features) const;

  std::string ToString() const override;

  size_t IncrementLevels() { return ++levels_; }

  size_t levels() const { return levels_; }

  ExpandType type() const { return type_; }

 private:
  ExpandType type_;
  size_t levels_;
};

// Struct to be used as a parameter to RedfishInterface implementations
struct GetParams {
  enum class Freshness { kOptional, kRequired };

  std::vector<const GetParamQueryInterface *> GetQueryParams() const {
    std::vector<const GetParamQueryInterface *> query_params;
    if (expand.has_value()) {
      query_params.push_back(&expand.value());
    }
    return query_params;
  }

  Freshness freshness = Freshness::kOptional;
  RedfishCachedGetterInterface::Relevance relevance =
      RedfishCachedGetterInterface::Relevance::kRelevant;
  bool auto_adjust_levels = false;
  std::optional<RedfishQueryParamExpand> expand;
};

// RedfishVariant is the standard return type for all Redfish interfaces.
// Its purpose is to force the caller to strictly specify the expected Redfish
// view to access the underlying Redfish payload internals.
//
// RedfishVariant must not be the sole owner of the Redfish data. Views
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
// RedfishVariant contains httpcode and httpheaders members. These fields are
// only populated if a HTTP request was required to produce the RedfishVariant.
// Some operations which construct a RedfishVariant may not involve HTTP
// requests (e.g. drilling down into child fields of a payload).
class RedfishVariant final {
 public:
  // Defines the mode used for iterable creation.
  // Setting it to kAllowExpand allows ImplIntf to re-read original RedFish
  // object with expand query parameters, if available
  // kDisableAutoResolve is the most restrictive iterable mode in which the
  // navigation properties are left unresolved.
  enum class IterableMode { kAllowExpand, kDisableExpand, kDisableAutoResolve };
  // ImplIntf is provided as the interface for subclasses to be implemented with
  // the PImpl idiom.

  class ImplIntf {
   public:
    virtual ~ImplIntf() {}
    virtual std::unique_ptr<RedfishObject> AsObject() const = 0;
    virtual std::unique_ptr<RedfishIterable> AsIterable(
        IterableMode mode, GetParams::Freshness freshness) const = 0;
    virtual std::optional<RedfishTransport::bytes> AsRaw() const = 0;
    virtual bool GetValue(std::string *val) const = 0;
    virtual bool GetValue(int32_t *val) const = 0;
    virtual bool GetValue(int64_t *val) const = 0;
    virtual bool GetValue(double *val) const = 0;
    virtual bool GetValue(bool *val) const = 0;
    virtual bool GetValue(absl::Time *val) const = 0;
    virtual std::string DebugString() const = 0;
    virtual void PrintDebugString() const {}
  };

  // Helper structures used with IndexHelper class
  // Denote a loop through an iterator.
  struct IndexEach {};
  // Denotes that item is a named element of the redfish schema and should be
  // read using GetArgs parameters
  struct IndexGetWithArgs {
    std::string name;
    GetParams args;
  };
  using IndexType =
      std::variant<std::string, size_t, IndexEach, IndexGetWithArgs>;

  // A helper class for lazy evaluation of an index operator chain, when
  // IndexEach is involved.
  class IndexHelper {
   public:
    IndexHelper() = delete;
    explicit IndexHelper(const RedfishVariant &root) : root_(root) {}
    IndexHelper(const RedfishVariant &root, IndexType index) : root_(root) {
      AppendIndex(index);
    }

    void AppendIndex(const IndexType &index) { indices_.push_back(index); }

    bool IsEmpty() const { return indices_.empty(); }

    IndexHelper Each(IterableMode mode = IterableMode::kAllowExpand) {
      AppendIndex(IndexEach{});
      return *this;
    }

    IndexHelper Get(std::string index, GetParams args = {}) {
      AppendIndex(IndexGetWithArgs{std::move(index), std::move(args)});
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
    RedfishIterReturnValue Do(F what) const {
      return Do(root_, indices_, what);
    }

   private:
    // Private helper to evaluate the entire chain recursively.
    template <typename F>
    RedfishIterReturnValue Do(const RedfishVariant &root,
                              absl::Span<const IndexType> indices,
                              F what) const;

    std::vector<IndexType> indices_;
    const RedfishVariant &root_;
  };

  // Construct from Status.
  explicit RedfishVariant(absl::Status status)
      : RedfishVariant(nullptr, std::move(status), absl::nullopt, {}) {}

  // Construct from ptr: the Redfish data is valid and there were no errors.
  explicit RedfishVariant(std::unique_ptr<ImplIntf> ptr)
      : RedfishVariant(std::move(ptr), absl::OkStatus(), absl::nullopt, {}) {}

  // Construct from httpcode + error object. A Status is constructed by
  // converting the provided httpcode.
  RedfishVariant(
      std::unique_ptr<ImplIntf> ptr, ecclesia::HttpResponseCode httpcode,
      const absl::flat_hash_map<std::string, std::string> &httpheaders)
      : RedfishVariant(
            std::move(ptr),
            absl::Status(ecclesia::HttpResponseCodeToCanonical(httpcode),
                         ecclesia::HttpResponseCodeToReasonPhrase(httpcode)),
            httpcode, httpheaders) {}

  RedfishVariant(const RedfishVariant &) = delete;
  RedfishVariant &operator=(const RedfishVariant &) = delete;
  RedfishVariant(RedfishVariant &&other) = default;
  RedfishVariant &operator=(RedfishVariant &&other) = default;

  inline RedfishVariant operator[](IndexGetWithArgs property) const;
  inline RedfishVariant operator[](const std::string &property) const;
  inline RedfishVariant operator[](size_t index) const;

  IndexHelper AsIndexHelper() const { return IndexHelper(*this); }

  IndexHelper Each() const {
    return IndexHelper(*this, IndexType(IndexEach()));
  }

  std::unique_ptr<RedfishObject> AsObject() const {
    if (!ptr_) {
      DLOG(INFO) << "RedfishVariant unable to create an Object: " << status();
      return nullptr;
    }
    return ptr_->AsObject();
  }
  std::unique_ptr<RedfishIterable> AsIterable(
      IterableMode mode = IterableMode::kAllowExpand,
      GetParams::Freshness freshness = GetParams::Freshness::kOptional) const {
    if (!ptr_) {
      DLOG(INFO) << "RedfishVariant unable to create an Iterable: " << status();
      return nullptr;
    }
    return ptr_->AsIterable(mode, freshness);
  }
  std::optional<RedfishTransport::bytes> AsRaw() const {
    if (!ptr_) {
      DLOG(INFO) << "RedfishVariant unable to create Raw: " << status();
      return std::nullopt;
    }
    return ptr_->AsRaw();
  }

  // This method will only return a valid object if this RedfishVariant
  // is a RedfishObject with an odata.id property that can be refetched with
  // a GET. If this prerequisite is met, then this method returns a
  // RedfishObject with data originating from the RedfishBackend and not a local
  // clientside cache.
  std::unique_ptr<RedfishObject> AsFreshObject() const;

  // Returns the status of the RedfishVariant.
  // Note that the status is independent from the Redfish Payload. See the
  // class-level docstring for more information.
  const absl::Status &status() const { return status_; }

  // Returns the httpcode, if one is available. See the class-level docstring
  // for more information.
  const std::optional<ecclesia::HttpResponseCode> &httpcode() const {
    return httpcode_;
  }

  // Returns the httpheaders, if one is available. See the class-level
  // docstring for more information.
  std::optional<absl::flat_hash_map<std::string, std::string>> httpheaders() {
    return httpheaders_;
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

  void PrintDebugString() const {
    if (!ptr_) return;
    ptr_->PrintDebugString();
  }

 private:
  RedfishVariant(
      std::unique_ptr<ImplIntf> ptr, absl::Status status,
      std::optional<ecclesia::HttpResponseCode> httpcode,
      std::optional<absl::flat_hash_map<std::string, std::string>> httpheaders)
      : ptr_(std::move(ptr)),
        status_(status),
        httpcode_(httpcode),
        httpheaders_(httpheaders) {}

  std::unique_ptr<ImplIntf> ptr_;
  absl::Status status_;
  std::optional<ecclesia::HttpResponseCode> httpcode_;
  std::optional<absl::flat_hash_map<std::string, std::string>> httpheaders_;
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

  // Returns the payload for a given named property node. Implementation is
  // similar to 'operator[](const std::string &node_name)' and extended with
  // GetParams arguments
  virtual RedfishVariant Get(const std::string &node_name,
                             GetParams params = {}) const = 0;

  // Returns the string URI of the current RedfishObject, if available.
  virtual std::optional<std::string> GetUriString() const = 0;

  // Returns a fresh copy of this RedfishObject. If this RedfishObject was a
  // cached object, this method will re-fetch this object with a GET. If this
  // RedfishObject was already a fresh instance, a copy of object itself will
  // be returned. This method will fail to return a valid RedfishVariant if this
  // RedfishObject does not have a string URI.
  virtual absl::StatusOr<std::unique_ptr<RedfishObject>> EnsureFreshPayload(
      GetParams params = {}) = 0;

  // Returns the content in the body of this object as a JSON. If the body
  // cannot be parsed as a JSON, nlohmann::json::value_t::discarded is returned.
  virtual nlohmann::json GetContentAsJson() const = 0;

  // Returns some implementation specific debug string. This should only be used
  // for logging and debugging and should not be fed into any parsers which
  // make assumptons on the underlying implementation.
  virtual std::string DebugString() const = 0;

  virtual void PrintDebugString() const {}

  // GetNodeValue is a convenience method which calls GetNode() then GetValue().
  // If the node does not exist or if the value could not be retrieved, nullopt
  // will be returned. Returns typed value or nullopt on error.
  template <typename T>
  std::optional<T> GetNodeValue(absl::string_view node_name) const {
    T val;
    auto node = (*this)[node_name.data()];
    if (!node.GetValue(&val)) return std::nullopt;
    return val;
  }
  template <typename PropertyDefinitionT>
  std::optional<typename PropertyDefinitionT::type> GetNodeValue() const {
    return GetNodeValue<typename PropertyDefinitionT::type>(
        PropertyDefinitionT::Name);
  }

  // ForEachProperty iterates over all properties in this object and invokes the
  // provided callback function. The callback has signature:
  //     RedfishIterReturnValue f(absl::string_view key, RedfishVariant value)
  //     key is the property name.
  //     value is the value returned as a RedfishVariant.
  // The function can return RedfishIterReturnValue::kContinue to continue
  // iterating over the remaining properties, or kStop to cease iterating over
  // any additional properties.
  virtual void ForEachProperty(
      absl::FunctionRef<RedfishIterReturnValue(absl::string_view key,
                                               RedfishVariant value)>) = 0;
};

// RedfishInterface provides initial access points to the Redfish resource tree.
class RedfishInterface {
 public:
  // ListValue, ObjectValue and ValueVariant provide abstractions for passing
  // input data to mutable operations (e.g. POST, PATCH, DELETE) without
  // exposing the underlying transport data format (e.g. JSON).
  struct ListValue;
  struct ObjectValue;

  using ValueVariant = std::variant<int, bool, std::string, const char *,
                                    double, ListValue, ObjectValue>;

  struct ListValue {
    std::vector<ValueVariant> items;
  };
  struct ObjectValue {
    std::vector<std::pair<std::string, ValueVariant>> items;
  };

  static inline absl::string_view ServiceRootToUri(
      ServiceRootUri service_root) {
    switch (service_root) {
      case (ServiceRootUri::kRedfish):
        return kServiceRoot;
      case (ServiceRootUri::kGoogle):
        return kGoogleServiceRoot;
    }
    // We use assert here to avoid g3 dependencies.
    LOG(FATAL) << "Unexpected value for Service Root";
  }

  virtual ~RedfishInterface() {}

  // An endpoint is trusted if all of the information coming from the endpoint
  // can be reliably assumed to be from a Google-controlled source.
  // Examples of trusted endpoints are attested BMCs and prodimage running in
  // caretaker mode.
  enum TrustedEndpoint { kTrusted, kUntrusted };

  // Updates the transport for sending Redfish requests.
  // API is deprected and the recommended way is to recreate the transport
  ABSL_DEPRECATED("Create a new instance instead")
  virtual void UpdateTransport(std::unique_ptr<RedfishTransport> new_transport,
                               TrustedEndpoint trusted) = 0;

  // Returns whether the endpoint is trusted.
  virtual bool IsTrusted() const = 0;

  // Fetches the root payload and returns it.
  virtual RedfishVariant GetRoot(
      GetParams params = {},
      ServiceRootUri service_root = ServiceRootUri::kRedfish) = 0;
  virtual RedfishVariant GetRoot(GetParams params,
                                 absl::string_view service_root) = 0;

  // The following Get URIs fetches the given URIs and returns the resulting
  // payloads. Both CachedGetUri and UncachedGetUri go through the cache
  // implementation, so calling UncachedGetUri may result in a cache update
  // depending on the cache implementation.
  //
  // When deciding whether to use one or the other, CachedGetUri is appropriate
  // if it is acceptable for the data to be stale according to the cache policy.
  // This typically includes information which will seldom change (e.g. resource
  // collections, data loaded on boot, addresses of devices which cannot be
  // hotplugged, manufacturing information, etc.) UncachedGetUri is intended for
  // live data which can change in real time (e.g. sensor readings, counters).
  virtual RedfishVariant CachedGetUri(absl::string_view uri,
                                      GetParams params = {}) = 0;
  virtual RedfishVariant UncachedGetUri(absl::string_view uri,
                                        GetParams params = {}) = 0;

  // Post to the given URI and returns result.
  virtual RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) = 0;

  // Post to the given URI and returns result.
  virtual RedfishVariant PostUri(absl::string_view uri,
                                 absl::string_view data) = 0;

  // Delete to the given URI and returns result.
  virtual RedfishVariant DeleteUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) = 0;

  // Delete to the given URI and returns result.
  virtual RedfishVariant DeleteUri(absl::string_view uri,
                                   absl::string_view data) = 0;

  // Post to the given URI and returns cached result.
  // The caller can specify the max duration for this particular POST operation
  // (keyed by uri + payload);
  // Unlike CachedGetUri, we allow each CachedPostUri(keyed by uri + payload) to
  // configure its own cache max duration, as each POST might have very its own
  // unique freshessness requirement. Note that only the max duration of first
  // call to each CachedPostUri has effect.
  virtual RedfishVariant CachedPostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span,
      absl::Duration duration) = 0;

  // Patch to the given URI and returns result.
  virtual RedfishVariant PatchUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) = 0;

  // Patch to the given URI and returns result.
  virtual RedfishVariant PatchUri(absl::string_view uri,
                                  absl::string_view data) = 0;

  virtual std::optional<RedfishSupportedFeatures> SupportedFeatures() const {
    return std::nullopt;
  }

 protected:
  static inline constexpr absl::string_view kServiceRoot = "/redfish/v1";
  static inline constexpr absl::string_view kGoogleServiceRoot = "/google/v1";
};

// Concrete implementation to provide a null placeholder interface which returns
// no data on all requests.
class NullRedfish : public RedfishInterface {
  // The transport cannot be updated in the null implementation. The transport
  // will never magically start working.
  void UpdateTransport(std::unique_ptr<RedfishTransport> new_transport,
                       TrustedEndpoint trusted) override {}
  // The null endpoint is trusted as it doesn't provide any system information,
  // so there is nothing it could lie about.
  bool IsTrusted() const override { return true; }
  RedfishVariant GetRoot(GetParams params,
                         ServiceRootUri service_root) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant GetRoot(GetParams params,
                         absl::string_view service_root) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant CachedGetUri(absl::string_view uri,
                              GetParams params) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant UncachedGetUri(absl::string_view uri,
                                GetParams params) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant PostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant DeleteUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant CachedPostUri(
      absl::string_view uri,
      absl::Span<const std::pair<std::string, ValueVariant>> kv_span,
      absl::Duration duration) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant PostUri(absl::string_view uri,
                         absl::string_view data) override {
    return RedfishVariant(absl::UnimplementedError("NullRedfish"));
  }
  RedfishVariant DeleteUri(absl::string_view uri,
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

RedfishVariant RedfishVariant::operator[](IndexGetWithArgs property) const {
  if (!status_.ok()) {
    return RedfishVariant(nullptr, status_, httpcode_, httpheaders_);
  }
  if (std::unique_ptr<RedfishObject> obj = AsObject()) {
    return (*obj).Get(property.name, std::move(property.args));
  }
  return RedfishVariant(absl::InternalError("not a RedfishObject"));
}

RedfishVariant RedfishVariant::operator[](const std::string &property) const {
  if (!status_.ok()) {
    return RedfishVariant(nullptr, status_, httpcode_, httpheaders_);
  }
  if (std::unique_ptr<RedfishObject> obj = AsObject()) {
    return (*obj)[property];
  }
  return RedfishVariant(absl::InternalError("not a RedfishObject"));
}

RedfishVariant RedfishVariant::operator[](const size_t index) const {
  if (!status_.ok()) {
    return RedfishVariant(nullptr, status_, httpcode_, httpheaders_);
  }
  if (std::unique_ptr<RedfishIterable> iter = AsIterable()) {
    return (*iter)[index];
  }
  return RedfishVariant(absl::InternalError("not a RedfishIterable"));
}

// Evaluates the index chain in a recursive fashion.
template <typename F>
RedfishIterReturnValue RedfishVariant::IndexHelper::Do(
    const RedfishVariant &root, absl::Span<const IndexType> indices,
    F what) const {
  if (indices.empty()) {
    // The chain is empty. That means we have evaluated the whole chain. Variant
    // `root` should be one of the objects the chain matches. So we will just
    // call `what` on it, and stop the recursion.
    if (std::unique_ptr<RedfishObject> obj = root.AsObject()) {
      return what(obj);
    }
    return RedfishIterReturnValue::kContinue;
  }

  // The chain is not empty. We will evaluate the 1st index in the chain, and
  // leave the rest to the next layer of recursion.
  const IndexType &index = indices[0];
  return std::visit(
      [&](auto &&index_value) -> RedfishIterReturnValue {
        using T = std::decay_t<decltype(index_value)>;
        auto rest = indices.last(indices.size() - 1);
        if constexpr (std::is_same_v<T, IndexGetWithArgs>) {
          if (index_value.args.expand.has_value() &&
              index_value.args.auto_adjust_levels) {
            // Number of levels should be incremented to cover elements in the
            // query after expand+auto_adjust_levels. The expectation is that
            // number of expands is similar to number of 'each' indexes.
            IndexGetWithArgs index_get = index_value;
            RedfishQueryParamExpand &expand = index_get.args.expand.value();
            for (const auto &rest_index : rest) {
              if (std::get_if<IndexEach>(&rest_index) != nullptr) {
                expand.IncrementLevels();
              }
            }
            if (Do(root[index_get], rest, what) ==
                RedfishIterReturnValue::kStop) {
              return RedfishIterReturnValue::kStop;
            }
            return RedfishIterReturnValue::kContinue;
          }
        }
        if constexpr (std::is_same_v<T, std::string> ||
                      std::is_same_v<T, IndexGetWithArgs> ||
                      std::is_same_v<T, size_t>) {
          // If the index is a string, likely we are looking at a RedfishObject.
          // If the index is an Expand, we are looking at a RedfishObject with
          // redfish expand options added.
          // If the index is an integer. We should be looking at a
          // RedfishIterable.
          // Simply drill down.
          if (Do(root[index_value], rest, what) ==
              RedfishIterReturnValue::kStop) {
            return RedfishIterReturnValue::kStop;
          }
        } else if constexpr (std::is_same_v<T, IndexEach>) {
          // This segment of the chain is an `Each()`. Therefore we are looking
          // at a RedfishIterable, and need to iterate over its elements. We
          // then drill down to each of the elements.
          auto iter = root.AsIterable();
          if (!iter) return RedfishIterReturnValue::kContinue;
          for (auto entry : *iter) {
            if (Do(entry, rest, what) == RedfishIterReturnValue::kStop) {
              return RedfishIterReturnValue::kStop;
            }
          }
        }
        return RedfishIterReturnValue::kContinue;
      },
      index);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_INTERFACE_H_
