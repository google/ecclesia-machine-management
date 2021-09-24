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

// This header provides a template that can be used to generated fixed-range
// integer wrappers. This encapsulates an integer value into a class which will
// guarantee that any instance of the class holds a value in the specified
// range.
//
// The expected way to utilize this class is by creating your own subclass using
// the standard pattern of:
//
//   class MyIntClass : public FixedRangeInteger<MyIntClass, int, XX, YY> {
//    public:
//     explicit constexpr MyIntClass(BaseType value) : BaseType(value) {}
//   };
//
// This will give you a MyIntClass that can store an integer that is guaranteed
// to be in the closed interval [XX, YY], for whatever values of XX and YY you
// specify. The underlying stored integer type can also be varied by modifying
// the second parameter (e.g. you can use "int", "long", "uint16_t", etc).
//
// Ideally, you would not even need to define a constructor. However, in order
// for the template to be able to create instances of your type it needs a
// public constructor and by default subclasses will not provide one. Thus you
// need to add this boilerplate to allow FixedRangeInteger to construct an
// instance of the subclass using its own copy constructor.
//
// The produced class will have the following features:
//   * Make(), a static factory function that constructs an instance of your
//     type from compile-time values. Value is passed as a template parameter.
//     It also provides overloads that accept run-time values, but only from
//     integer types whose max and min fall within the types range.
//   * TryMake(), a static factory function that constructs an instance of your
//     type from run-time values. It can fail if the values are out of range and
//     so it returns an optional which will be nullopt in that case.
//   * Clamp(), a static factory function that constructs a instance of your
//     type from frompile-time values. It would clamp the values if the values
//     are out of range.
//   * value(), an accessor that returns the stored value
//   * Copy, Copy-Assign, Move, and Move-Assign operators
//   * Relational operators, that match relations on the underlying value.
//   * Hash support, that just hashes using the underlying integer.
//
// The produced class does not implement integer bitwise or arithmetic
// operators. There is no clear reasonable semantics to apply when the result
// of such an operation is out of range and so rather than attempting this it
// is simply not supported. Clients that need to perform arithmetic on these
// can perform arithmetic on the stored values, handling the out of range
// results as best suits their use case.

#ifndef ECCLESIA_LIB_TYPES_FIXED_RANGE_INT_H_
#define ECCLESIA_LIB_TYPES_FIXED_RANGE_INT_H_

#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace ecclesia {

template <typename T, typename IntType, IntType MinValue, IntType MaxValue>
class FixedRangeInteger {
 public:
  // The base type of subclasses of this template. Useful to simplify the
  // definition of the subclass constructor without having to repeat all of the
  // template parameters.
  using BaseType = FixedRangeInteger<T, IntType, MinValue, MaxValue>;

  // The underlying integer type.
  using StoredType = IntType;

  // The min and max range this type enforces.
  static constexpr IntType kMinValue = MinValue;
  static constexpr IntType kMaxValue = MaxValue;

  // Compile-time factory function. Enforces the input range via static_assert.
  template <IntType Value>
  static constexpr T Make() {
    static_assert(Value >= kMinValue && Value <= kMaxValue,
                  "fixed range integer value out of the valid range");
    return T(BaseType(Value));
  }

  // Overload of the compile-time factory function that accepts runtime values
  // but only from integer types that are guaranteed to fall within the valid
  // range of the type. So for example if you have a type with a range that
  // spans [0, 65535] then you can construct it directly from a uint16_t.
  template <typename IntParamType,
            std::enable_if_t<
                std::is_integral_v<IntParamType> &&
                    std::numeric_limits<IntParamType>::min() >= MinValue &&
                    std::numeric_limits<IntParamType>::max() <= MaxValue,
                int> = 0>
  static constexpr T Make(IntParamType value) {
    return T(BaseType(value));
  }

  // Run-time factory function. Returns nullopt if the given value is out of
  // range, otherwise returns the value of T.
  static std::optional<T> TryMake(IntType value) {
    if (value >= kMinValue && value <= kMaxValue) {
      return T(BaseType(value));
    } else {
      return std::nullopt;
    }
  }

  // Run-time factory function. Returns kMaxValue if value is greater than
  // kMaxValue, returns kMinValue if value is less than kMinValue, otherwise
  // returns the value.
  static T Clamp(IntType value) {
    if (value > kMaxValue) {
      return T(BaseType(kMaxValue));
    }
    if (value < kMinValue) {
      return T(BaseType(kMinValue));
    }
    return T(BaseType(value));
  }

  // Value accessor.
  IntType value() const { return value_; }

  // Relational operators. Ordering uses the underlying integer order.
  friend bool operator==(const T &lhs, const T &rhs) {
    return lhs.value_ == rhs.value_;
  }
  friend bool operator!=(const T &lhs, const T &rhs) {
    return lhs.value_ != rhs.value_;
  }
  friend bool operator<(const T &lhs, const T &rhs) {
    return lhs.value_ < rhs.value_;
  }
  friend bool operator>(const T &lhs, const T &rhs) {
    return lhs.value_ > rhs.value_;
  }
  friend bool operator<=(const T &lhs, const T &rhs) {
    return lhs.value_ <= rhs.value_;
  }
  friend bool operator>=(const T &lhs, const T &rhs) {
    return lhs.value_ >= rhs.value_;
  }

  // Support hashing of values. Just hashes the underlying integer.
  template <typename H>
  friend H AbslHashValue(H h, const T &t) {
    return H::combine(std::move(h), t.value_);
  }

 private:
  // Unchecked constructor. Only invoked by the static factory functions which
  // all check the range of value before calling this. This is private so that
  // users and subclasses CANNOT construct this directly.
  explicit constexpr FixedRangeInteger(IntType value) : value_(value) {}

  // The stored value. Guaranteed to be in range [kMinValue, kMaxValue].
  IntType value_;
};

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_TYPES_FIXED_RANGE_INT_H_
