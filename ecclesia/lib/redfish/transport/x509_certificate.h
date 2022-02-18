/*
 * Copyright 2022 Google LLC
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

#ifndef ECCLESIA_LIB_REDFISH_TRANSPORT_X509_CERTIFICATE_H_
#define ECCLESIA_LIB_REDFISH_TRANSPORT_X509_CERTIFICATE_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "openssl/base.h"
#include "openssl/x509.h"

namespace ecclesia {

// Subject Alternative Names (SANs). SAN is an extension to X.509 that allows
// various values to be associated with a security certificate: Email addresses,
// IP addresses, URIs, DNS names, etc.
struct SubjectAltName {
  // |spiffe_id| is a URI SAN, used in Google internal services.
  absl::optional<std::string> spiffe_id = absl::nullopt;
  // |fqdn| is a DNS SAN.
  absl::optional<std::string> fqdn = absl::nullopt;
  // |critical| represents whether these SANs are critical. See
  // https://www.openssl.org/docs/manmaster/man5/x509v3_config.html
  bool critical = false;

  std::string DebugInfo() const {
    return absl::StrCat("spiffe_id=", spiffe_id.value_or("empty"),
                        "; fqdn=", fqdn.value_or("empty"),
                        "; critical=", critical ? "true" : "false");
  }
};

// Parses the Subject Altenative Name (SAN) from the given PEM certificate
// block. If there are multiple certificates in the block, it only parses the
// first one.
absl::StatusOr<SubjectAltName> GetSubjectAltName(absl::string_view pem);

}  // namespace ecclesia

#endif  // ECCLESIA_LIB_REDFISH_TRANSPORT_X509_CERTIFICATE_H_
