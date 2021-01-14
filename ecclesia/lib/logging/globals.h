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

// Definitions of the global objects used by the logging system, along with
// functions for getting and setting them. This header is normally only needed
// if you want to install your own logging implementation.
//
// There are three major things provided by this library:
//
// 1. A default logging implementation, set up when the logging system is first
//    constructed. This does minimal logging to stderr and is normally intended
//    to be replaced with something more fully featured.
//
// 2. Some objects that can be used to implement the actual user logging
//    functions. The LogMessageStream class is used as a temporary object in
//    log invocations to capture the logging text, and the LoggingStreamFactory
//    is used to wrap the LoggingInterface and construct LogMessageStream
//    instances. These object should never really be used directly by either
//    users _or_ backend implementers.
//
// 3. (Get|Set)GlobalLogger functions for installing a global logger, or getting
//    access to it to do logging.
//
// It is important to note that installing a new logger is not threadsafe. This
// generally means that you should not use SetGlobalLogger while a program is
// running multiple threads. Calling SetGlobalLogger will destroy the logging
// object pointed to by every outstanding LogMessageStream and so it is only
// safe to call if nothing is using the logging yet.
//
// It is okay to use the logging system before SetGlobalLogger is called (e.g.
// to log errors that occur while trying to set up the logger) so log as all
// logging is finished before the call is made.

#ifndef ECCLESIA_LIB_LOGGING_GLOBALS_H_
#define ECCLESIA_LIB_LOGGING_GLOBALS_H_

#include <iostream>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/synchronization/mutex.h"
#include "ecclesia/lib/logging/interfaces.h"
#include "ecclesia/lib/time/clock.h"

namespace ecclesia {

// The default logger implementation. Its behavior is:
//   * L0 logs go to stderr and then terminate the program
//   * L1 logs go to stderr
//   * L2+ logs are discarded
// This is the logger that will be added to the global LogMessageStream on
// startup. Programs desiring different behavior should install their own logger
// implementation to replace it.
class DefaultLogger : public LoggerInterface {
 public:
  explicit DefaultLogger(Clock *clock);

 private:
  void Write(WriteParameters params) override;

  // Used to synchronize logging.
  absl::Mutex mutex_;
  // Used to capture timestamps for log lines.
  Clock *clock_;
};

// Object that absorbs streamed text from users. When the various *Log()
// functions are called an instance of this object will be returned and the
// caller can write to it via the << operator.
//
// The logging will all be written out when the LogMessageStream object is
// destroyed. Callers should not hold onto the object outside of the logging
// statement, and so the lifetime should generally end when the logging
// statement does.
class LogMessageStream {
 public:
  // When the LogMessage is destroyed it will flush its contents out to the
  // underlying logger.
  ~LogMessageStream() { Flush(); }

  // You can't copy or assign to a message stream, but you can move it. This is
  // because message stream is basically intended to be used as a temporary
  // object that accumulates data as it is written to and then flushes it out to
  // the logger when it is destroyed. Thus, we delete all of these operators
  // except for the move, which allows values to be efficiently returned.
  LogMessageStream(const LogMessageStream &other) = delete;
  LogMessageStream &operator=(const LogMessageStream &other) = delete;
  LogMessageStream(LogMessageStream &&other)
      : log_level_(other.log_level_),
        source_location_(other.source_location_),
        logger_(other.logger_),
        stream_(std::move(other.stream_)) {
    // Explicitly clear out the logger from the moved-from object. This keeps it
    // from flushing anything to the logger in its destructor.
    other.logger_ = nullptr;
  }
  LogMessageStream &operator=(LogMessageStream &&other) = delete;

  // Manually flush the log message stream out. After this is called the logger
  // will be set to nullptr and subsequent flushes will do nothing.
  void Flush() {
    if (logger_) {
      logger_->Write({.log_level = log_level_,
                      .source_location = source_location_,
                      .text = stream_.str()});
      logger_ = nullptr;
    }
  }

  // Implement the stream operator (<<) for the log message stream. Under the
  // covers this writes to the underlying ostringstream.
  template <typename T>
  LogMessageStream &operator<<(T &&value) {
    stream_ << std::forward<T>(value);
    return *this;
  }

 private:
  // Used to give LoggerStreamFactory permission to construct these objects.
  friend class LoggerStreamFactory;

  LogMessageStream(int log_level, SourceLocation source_location,
                   LoggerInterface *logger)
      : log_level_(log_level),
        source_location_(source_location),
        logger_(logger) {}

  // The information and objects need to actually write out the log.
  int log_level_;
  SourceLocation source_location_;
  LoggerInterface *logger_;

  // A string stream used to accumulate the log text.
  std::ostringstream stream_;
};

// CRTP style wrapper class that can be used to wrap a log messages stream
// and hook additional behavior into its destruction, in particular appending
// messages to the stream _after_ the user messages. Classes which use this can
// do this by implementing their own destructor.
template <typename T>
class WrappedLogMessageStream {
 public:
  // Subclasses must pass in a refernece to themselves as well as the wrapped
  // stream object. The reference is needed to support generic functions that
  // return a reference to T.
  WrappedLogMessageStream(T &true_this, LogMessageStream lms)
      : true_this_(true_this), lms_(std::move(lms)) {}

  template <typename V>
  T &operator<<(V &&value) {
    lms_ << std::forward<V>(value);
    return true_this_;
  }

 protected:
  T &true_this_;
  LogMessageStream lms_;
};

// Wrapper around LogMessageStream that will abort in its destructor. The
// destructor is marked as noreturn so that the compiler will understand that
// execution will not proceed after a fatal logging line.
class LogMessageStreamAndAbort
    : public WrappedLogMessageStream<LogMessageStreamAndAbort> {
 public:
  explicit LogMessageStreamAndAbort(LogMessageStream lms);

  LogMessageStreamAndAbort(const LogMessageStreamAndAbort &other) = delete;
  LogMessageStreamAndAbort &operator=(const LogMessageStreamAndAbort &other) =
      delete;

  ABSL_ATTRIBUTE_NORETURN ~LogMessageStreamAndAbort();
};

// Class that wraps a LoggerInterface implementation that can be used to
// construct logging streams. Used as a global singleton.
class LoggerStreamFactory {
 public:
  // Construct a new factory using the given logger.
  LoggerStreamFactory(std::unique_ptr<LoggerInterface> logger);

  // Replace the current logger implementation with an new one. This code is not
  // thread-safe, and is not safe to call if there are any outstanding
  // LogMessageStream objects.
  void SetLogger(std::unique_ptr<LoggerInterface> logger);

  // Create a new message stream for logging at a particular level. The stream
  // will be prepended with log level and timestamp text.
  LogMessageStream MakeStream(int log_level, SourceLocation loc) const {
    return LogMessageStream(log_level, loc, logger_.get());
  }

  // Create a new message stream that will absorb streams but not actually send
  // them anywhere. This is useful for conditional logging functions that need
  // to return an object for streaming to but which only actuall use the logging
  // on some branches.
  LogMessageStream MakeNullStream() const {
    return LogMessageStream(-1, SourceLocation::current(), nullptr);
  }

 private:
  // The underlying log implementation stored in this object.
  std::unique_ptr<LoggerInterface> logger_;
};

// Get the global logger. Note that this returns a reference to the stream
// factory, and not the underlying LoggerInterface.
const LoggerStreamFactory &GetGlobalLogger();

// Install a new global logger implementation. This cannot be done safely while
// logging being done and so should normally only be done very early in process
// startup, before you have any multi-threading.
void SetGlobalLogger(std::unique_ptr<LoggerInterface> logger);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_LOGGING_GLOBALS_H_
