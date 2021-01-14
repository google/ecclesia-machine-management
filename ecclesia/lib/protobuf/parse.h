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

// Helpful functions for parsing text protobufs. This provide useful short forms
// that are useful in places like tests where you have known data and it's safe
// to just hard-terminate the program when parsing fails.
//
// ParseTextProtoOrDie(text):
//   Parses "text" as a message where the type is automatically inferred from
//   the type the result is being assigned to.
//
// ParseTextAsProtoOrDie<MessageType>(text):
//   Parses "text" as the explicitly defined message type.

#ifndef ECCLESIA_LIB_PROTOBUF_PARSE_H_
#define ECCLESIA_LIB_PROTOBUF_PARSE_H_

#include <string>
#include <vector>

#include "google/protobuf/io/tokenizer.h"
#include "google/protobuf/text_format.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/logging/logging.h"

namespace ecclesia {

// Parse a text proto and terminate with a fatal log if parsing fails. This
// captures the source location of the caller so that failures are associated
// with the code doing the parse.
template <typename MessageType>
MessageType ParseTextAsProtoOrDie(
    const std::string &text, SourceLocation loc = SourceLocation::current()) {
  // Define a collector that just accumulates a list of error message strings.
  class TextErrorCollector : public ::google::protobuf::io::ErrorCollector {
   public:
    const std::vector<std::string> &errors() const { return errors_; }

   private:
    void AddError(int line, int column, const std::string &message) override {
      errors_.push_back(
          absl::StrFormat("line %d, column %d: %s", line, column, message));
    }
    void AddWarning(int line, int column, const std::string &message) override {
      errors_.push_back(
          absl::StrFormat("line %d, column %d: %s", line, column, message));
    }

    std::vector<std::string> errors_;
  };

  // Set up a parser using our collector.
  TextErrorCollector collector;
  ::google::protobuf::TextFormat::Parser parser;
  parser.RecordErrorsTo(&collector);

  // Try the actual parse. If it fails, terminate with all of the errors.
  MessageType message;
  if (!parser.ParseFromString(text, &message)) {
    FatalLog(loc) << "text proto parsing failed:\n"
                  << absl::StrJoin(collector.errors(), "\n");
  }
  return message;
}

// Temporary object used by ParseTextProtoOrDie to hold the result. This defers
// the parsing until the result is assigned to a return type. Do not try to
// capture these objects yourself, either explicitly or via auto; if you're not
// assigning the result of ParseTextProtoOrDie to a protobuf value type then
// you're doing it wrong.
class ParseTextProtoOrDieTemporary {
 public:
  // Automatically convert this to the protobuf message type it is assigned to.
  template <typename T>
  operator T() {
    return ParseTextAsProtoOrDie<T>(text_, loc_);
  }

 private:
  // Only our parsing function should be able to construct these.
  friend ParseTextProtoOrDieTemporary ParseTextProtoOrDie(const std::string &,
                                                          SourceLocation);

  ParseTextProtoOrDieTemporary(const std::string &text, SourceLocation loc)
      : text_(text), loc_(loc) {}

  // We don't want anyone copying or passing these objects around.
  ParseTextProtoOrDieTemporary(const ParseTextProtoOrDieTemporary &) = delete;
  ParseTextProtoOrDieTemporary &operator=(
      const ParseTextProtoOrDieTemporary &) = delete;

  // Normally we don't want to capture references but since this type is only
  // ever supposed to be used as a temporary in an expression it should be safe.
  const std::string &text_;
  SourceLocation loc_;
};

// Untyped version of ParseTextAsProtoOrDie that uses the return type of the
// function to determine the type to parse to.
inline ParseTextProtoOrDieTemporary ParseTextProtoOrDie(
    const std::string &text, SourceLocation loc = SourceLocation::current()) {
  return ParseTextProtoOrDieTemporary(text, loc);
}

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_PROTOBUF_PARSE_H_
