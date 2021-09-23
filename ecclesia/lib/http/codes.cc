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

#include "ecclesia/lib/http/codes.h"

#include <cstddef>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace ecclesia {
namespace {

// Sorted list of all supported HTTP response codes.
constexpr int kResponseCodes[] = {
    100, 101, 102, 200, 201, 202, 203, 204, 205, 206, 207, 208, 226,
    300, 301, 302, 303, 304, 305, 307, 308, 400, 401, 402, 403, 404,
    405, 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417,
    418, 421, 422, 423, 424, 425, 426, 428, 429, 431, 451, 499, 500,
    501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511,
};
// Helper template to ensure kResponseCodes is sorted at compile-time.
template <typename T>
constexpr bool array_is_sorted(const T &array) {
  static_assert(sizeof(array) > 0, "array must not be empty");
  size_t array_len = sizeof(array) / sizeof(array[0]);
  for (size_t i = 1; i < array_len; ++i) {
    if (array[i] < array[i - 1]) return false;
  }
  return true;
}

}  // namespace

absl::string_view HttpResponseCodeToReasonPhrase(HttpResponseCode rc) {
  switch (rc) {
    // 100 range: Informational
    case HTTP_CODE_CONTINUE:
      return "Continue";
    case HTTP_CODE_SWITCHING:
      return "Switching Protocols";
    case HTTP_CODE_PROCESSING:
      return "Processing";

    // 200 range: Success
    case HTTP_CODE_REQUEST_OK:
      return "OK";
    case HTTP_CODE_CREATED:
      return "Created";
    case HTTP_CODE_ACCEPTED:
      return "Accepted";
    case HTTP_CODE_PROVISIONAL:
      return "Non-Authoritative Information";
    case HTTP_CODE_NO_CONTENT:
      return "No Content";
    case HTTP_CODE_RESET_CONTENT:
      return "Reset Content";
    case HTTP_CODE_PART_CONTENT:
      return "Partial Content";
    case HTTP_CODE_MULTI_STATUS:
      return "Multi-Status";
    case HTTP_CODE_ALREADY_REPORTED:
      return "Already Reported";
    case HTTP_CODE_IM_USED:
      return "IM Used";

    // 300 range: redirects
    case HTTP_CODE_MULTIPLE:
      return "Multiple Choices";
    case HTTP_CODE_MOVED_PERM:
      return "Moved Permanently";
    case HTTP_CODE_MOVED_TEMP:
      return "Found";
    case HTTP_CODE_SEE_OTHER:
      return "See Other";
    case HTTP_CODE_NOT_MODIFIED:
      return "Not Modified";
    case HTTP_CODE_USE_PROXY:
      return "Use Proxy";
    case HTTP_CODE_TEMP_REDIRECT:
      return "Temporary Redirect";
    case HTTP_CODE_RESUME_INCOMPLETE:
      return "Resume Incomplete";

    // 400 range: client errors
    case HTTP_CODE_BAD_REQUEST:
      return "Bad Request";
    case HTTP_CODE_UNAUTHORIZED:
      return "Unauthorized";
    case HTTP_CODE_PAYMENT:
      return "Payment Required";
    case HTTP_CODE_FORBIDDEN:
      return "Forbidden";
    case HTTP_CODE_NOT_FOUND:
      return "Not Found";
    case HTTP_CODE_METHOD_NA:
      return "Method Not Allowed";
    case HTTP_CODE_NONE_ACC:
      return "Not Acceptable";
    case HTTP_CODE_PROXY:
      return "Proxy Authentication Required";
    case HTTP_CODE_REQUEST_TO:
      return "Request Timeout";
    case HTTP_CODE_CONFLICT:
      return "Conflict";
    case HTTP_CODE_GONE:
      return "Gone";
    case HTTP_CODE_LEN_REQUIRED:
      return "Length Required";
    case HTTP_CODE_PRECOND_FAILED:
      return "Precondition Failed";
    case HTTP_CODE_ENTITY_TOO_BIG:
      return "Request Entity Too Large";
    case HTTP_CODE_URI_TOO_BIG:
      return "Request-URI Too Large";
    case HTTP_CODE_UNKNOWN_MEDIA:
      return "Unsupported Media Type";
    case HTTP_CODE_BAD_RANGE:
      return "Requested range not satisfiable";
    case HTTP_CODE_BAD_EXPECTATION:
      return "Expectation Failed";
    // RFCs 2324, 7168.
    case HTTP_CODE_IM_A_TEAPOT:
      return "I'm a Teapot";
    case HTTP_CODE_MISDIRECTED_REQUEST:
      return "Misdirected Request";
    case HTTP_CODE_UNPROC_ENTITY:
      return "Unprocessable Entity";
    case HTTP_CODE_LOCKED:
      return "Locked";
    case HTTP_CODE_FAILED_DEP:
      return "Failed Dependency";
    case HTTP_CODE_TOO_EARLY:
      return "Too Early";
    case HTTP_CODE_UPGRADE_REQUIRED:
      return "Upgrade Required";
    case HTTP_CODE_PRECOND_REQUIRED:
      return "Precondition Required";
    case HTTP_CODE_TOO_MANY_REQUESTS:
      return "Too Many Requests";
    case HTTP_CODE_HEADER_TOO_LARGE:
      return "Request Header Fields Too Large";
    case HTTP_CODE_UNAVAILABLE_LEGAL:
      return "Unavailable For Legal Reasons";
    case HTTP_CODE_CLIENT_CLOSED_REQUEST:
      return "Client Closed Request";

    // 500 range: server errors
    case HTTP_CODE_ERROR:
      return "Internal Server Error";
    case HTTP_CODE_NOT_IMP:
      return "Not Implemented";
    case HTTP_CODE_BAD_GATEWAY:
      return "Bad Gateway";
    case HTTP_CODE_SERVICE_UNAV:
      return "Service Unavailable";
    case HTTP_CODE_GATEWAY_TO:
      return "Gateway Timeout";

    case HTTP_CODE_BAD_VERSION:
      return "HTTP Version not supported";
    case HTTP_CODE_VARIANT_NEGOTIATES:
      return "Variant Also Negotiates";
    case HTTP_CODE_INSUF_STORAGE:
      return "Insufficient Storage";
    case HTTP_CODE_LOOP_DETECTED:
      return "Loop Detected";
    case HTTP_CODE_BANDWIDTH_EXCEEDED:
      return "Bandwidth Limit Exceeded";
    case HTTP_CODE_NOT_EXTENDED:
      return "Not Extended";
    case HTTP_CODE_NETAUTH_REQUIRED:
      return "Network Authentication Required";

    case HTTP_CODE_UNDEFINED:
      break;
    case HTTP_CODE_LAST_CODE:
      break;
      // To add a new status code, see above, code.h's enum, and add to
      // response_codes[]
  }
  // We don't have a name for this response code, so we'll just
  // take the blame
  return "Internal Server Error";
}

HttpResponseCode HttpResponseCodeFromInt(int code) {
  static_assert(array_is_sorted(kResponseCodes), "HTTP codes must be sorted");
  if (absl::c_binary_search(kResponseCodes, code)) {
    return static_cast<HttpResponseCode>(code);
  }
  return HTTP_CODE_UNDEFINED;
}

absl::StatusCode HttpResponseCodeToCanonical(HttpResponseCode code) {
  // This should be kept in sync with the mapping documented in
  // googleapis/google/rpc/code.proto.
  switch (code) {
    case HTTP_CODE_BAD_REQUEST:
      return absl::StatusCode::kInvalidArgument;
    case HTTP_CODE_FORBIDDEN:
      return absl::StatusCode::kPermissionDenied;
    case HTTP_CODE_NOT_FOUND:
      return absl::StatusCode::kNotFound;
    case HTTP_CODE_CONFLICT:
      return absl::StatusCode::kAborted;
    case HTTP_CODE_BAD_RANGE:
      return absl::StatusCode::kOutOfRange;
    case HTTP_CODE_TOO_MANY_REQUESTS:
      return absl::StatusCode::kResourceExhausted;
    case HTTP_CODE_CLIENT_CLOSED_REQUEST:
      return absl::StatusCode::kCancelled;
    case HTTP_CODE_GATEWAY_TO:
      return absl::StatusCode::kDeadlineExceeded;
    case HTTP_CODE_NOT_IMP:
      return absl::StatusCode::kUnimplemented;
    case HTTP_CODE_SERVICE_UNAV:
      return absl::StatusCode::kUnavailable;
    case HTTP_CODE_UNAUTHORIZED:
      return absl::StatusCode::kUnauthenticated;
    default: {
      if (code >= 200 && code < 300) return absl::StatusCode::kOk;
      if (code >= 400 && code < 500)
        return absl::StatusCode::kFailedPrecondition;
      if (code >= 500 && code < 600) return absl::StatusCode::kInternal;
      return absl::StatusCode::kUnknown;
    }
  }
}

}  // namespace ecclesia
