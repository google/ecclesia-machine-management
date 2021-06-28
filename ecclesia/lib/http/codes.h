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

#ifndef ECCLESIA_LIB_HTTP_CODES_H_
#define ECCLESIA_LIB_HTTP_CODES_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace ecclesia {

enum HttpResponseCode {     // these are HTTP/1.0 responses, mostly
  HTTP_CODE_UNDEFINED = 0,  // illegal value for initialization
  HTTP_CODE_FIRST_CODE = 100,

  // Informational
  HTTP_CODE_CONTINUE = 100,    // Continue
  HTTP_CODE_SWITCHING = 101,   // Switching Protocols
  HTTP_CODE_PROCESSING = 102,  // Processing (rfc 2518, sec 10.1)

  // Success
  HTTP_CODE_REQUEST_OK = 200,        // OK
  HTTP_CODE_CREATED = 201,           // Created
  HTTP_CODE_ACCEPTED = 202,          // Accepted
  HTTP_CODE_PROVISIONAL = 203,       // Non-Authoritative Information
  HTTP_CODE_NO_CONTENT = 204,        // No Content
  HTTP_CODE_RESET_CONTENT = 205,     // Reset Content
  HTTP_CODE_PART_CONTENT = 206,      // Partial Content
  HTTP_CODE_MULTI_STATUS = 207,      // Multi-Status (rfc 2518, sec 10.2)
  HTTP_CODE_ALREADY_REPORTED = 208,  // Already Reported (rfc 5842)

  HTTP_CODE_IM_USED = 226,  // IM Used (rfc 3229)

  // Redirect
  HTTP_CODE_MULTIPLE = 300,           // Multiple Choices
  HTTP_CODE_MOVED_PERM = 301,         // Moved Permanently
  HTTP_CODE_MOVED_TEMP = 302,         // Found. For historical reasons,
                                      // a user agent MAY change the method
                                      // from POST to GET for the subsequent
                                      // request.
                                      // (rfc 7231, sec 6.4.3)
  HTTP_CODE_SEE_OTHER = 303,          // See Other
  HTTP_CODE_NOT_MODIFIED = 304,       // Not Modified
  HTTP_CODE_USE_PROXY = 305,          // Use Proxy
  HTTP_CODE_TEMP_REDIRECT = 307,      // Temporary Redirect. Similar to 302,
                                      // except that user agents MUST NOT
                                      // change the request method.
                                      // (rfc 7231, sec 6.4.7)
  HTTP_CODE_PERM_REDIRECT = 308,      // Permanent Redirect. Similar to 301.
                                      // (rfc 7538)
  HTTP_CODE_RESUME_INCOMPLETE = 308,  // Resume Incomplete. Google-specific.
  // (https://developers.google.com/drive/api/v3/manage-uploads#resumable)

  // Client Error
  HTTP_CODE_BAD_REQUEST = 400,          // Bad Request
  HTTP_CODE_UNAUTHORIZED = 401,         // Unauthorized
  HTTP_CODE_PAYMENT = 402,              // Payment Required
  HTTP_CODE_FORBIDDEN = 403,            // Forbidden
  HTTP_CODE_NOT_FOUND = 404,            // Not Found
  HTTP_CODE_METHOD_NA = 405,            // Method Not Allowed
  HTTP_CODE_NONE_ACC = 406,             // Not Acceptable
  HTTP_CODE_PROXY = 407,                // Proxy Authentication Required
  HTTP_CODE_REQUEST_TO = 408,           // Request Timeout
  HTTP_CODE_CONFLICT = 409,             // Conflict
  HTTP_CODE_GONE = 410,                 // Gone
  HTTP_CODE_LEN_REQUIRED = 411,         // Length Required
  HTTP_CODE_PRECOND_FAILED = 412,       // Precondition Failed
  HTTP_CODE_ENTITY_TOO_BIG = 413,       // Request Entity Too Large
  HTTP_CODE_URI_TOO_BIG = 414,          // Request-URI Too Large
  HTTP_CODE_UNKNOWN_MEDIA = 415,        // Unsupported Media Type
  HTTP_CODE_BAD_RANGE = 416,            // Requested range not satisfiable
  HTTP_CODE_BAD_EXPECTATION = 417,      // Expectation Failed
  HTTP_CODE_IM_A_TEAPOT = 418,          // I'm a Teapot (RFC 2324, 7168)
  HTTP_CODE_MISDIRECTED_REQUEST = 421,  // Misdirected Request (RFC 7540)
  HTTP_CODE_UNPROC_ENTITY = 422,        // Unprocessable Entity
                                        // (rfc 2518, sec 10.3)
  HTTP_CODE_LOCKED = 423,               // Locked (rfc 2518, sec 10.4)
  HTTP_CODE_FAILED_DEP = 424,           // Failed Dependency
                                        // (rfc 2518, sec 10.5)
  HTTP_CODE_TOO_EARLY = 425,            // Too Early
                                        // (rfc 8470)
  HTTP_CODE_UPGRADE_REQUIRED = 426,     // Upgrade Required
                                        // (rfc 7231, sec 6.5.14)
  HTTP_CODE_PRECOND_REQUIRED = 428,     // Precondition Required
                                        // (rfc 6585, sec 3)
  HTTP_CODE_TOO_MANY_REQUESTS = 429,    // Too Many Requests
                                        // (rfc 6585, sec 4)
  HTTP_CODE_HEADER_TOO_LARGE = 431,     // Request Header Fields Too Large
                                        // (rfc 6585, sec 5)
  HTTP_CODE_UNAVAILABLE_LEGAL = 451,    // Unavailable For Legal Reasons
                                        // (rfc 7725)

  HTTP_CODE_CLIENT_CLOSED_REQUEST = 499,  // Client Closed Request (Nginx)

  // Server Error
  HTTP_CODE_ERROR = 500,               // Internal Server Error
  HTTP_CODE_NOT_IMP = 501,             // Not Implemented
  HTTP_CODE_BAD_GATEWAY = 502,         // Bad Gateway
  HTTP_CODE_SERVICE_UNAV = 503,        // Service Unavailable
  HTTP_CODE_GATEWAY_TO = 504,          // Gateway Timeout
  HTTP_CODE_BAD_VERSION = 505,         // HTTP Version not supported
  HTTP_CODE_VARIANT_NEGOTIATES = 506,  // Variant Also Negotiates (rfc 2295)
  HTTP_CODE_INSUF_STORAGE = 507,       // Insufficient Storage
                                       // (rfc 2518, sec 10.6)
  HTTP_CODE_LOOP_DETECTED = 508,       // Loop Detected (rfc 5842)
  HTTP_CODE_BANDWIDTH_EXCEEDED = 509,  // Bandwidth Limit Exceeded (cPanel)
  // (https://documentation.cpanel.net/display/CKB/HTTP+Error+Codes+and+Quick+Fixes#HTTPErrorCodesandQuickFixes-509BandwidthLimitExceeded)
  HTTP_CODE_NOT_EXTENDED = 510,      // Not Extended (rfc 2774)
  HTTP_CODE_NETAUTH_REQUIRED = 511,  // Network Authentication Required
                                     // (rfc 6585, sec 6)

  HTTP_CODE_LAST_CODE = 599,
};

// Helper function converting an integer to a valid HTTPResponse enum, or
// returning HTTP_CODE_UNDEFINED if the value is not recognized.
HttpResponseCode HttpResponseCodeFromInt(int code);

// Return the appropriate human-readable reason phrase for the given status
// code.
absl::string_view HttpResponseCodeToReasonPhrase(HttpResponseCode rc);

// Converts the given status to canonical error code.
absl::StatusCode HttpResponseCodeToCanonical(HttpResponseCode code);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_HTTP_CODES_H_
