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

#include "ecclesia/lib/redfish/transport/x509_certificate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "ecclesia/lib/status/macros.h"
#include "openssl/base.h"
#include "openssl/pem.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"

namespace ecclesia {

namespace {

std::string GetOpenSslError() {
  char err_buf[ERR_ERROR_STRING_BUF_LEN]{};
  ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
  return err_buf;
}

// Returns short name of the corresponding numeric identifier.
std::string GetNidShortName(int nid) {
  const char* short_name = OBJ_nid2sn(nid);
  if (short_name == nullptr) {
    return absl::StrCat("Unknown NID: ", nid);
  }
  return short_name;
}

absl::StatusOr<bssl::UniquePtr<GENERAL_NAMES>> GetSubjectAltNameExtension(
    const X509& cert, int& out_critical) {
  // Find and decode the extension of type |GENERAL_NAMES|
  // The const_cast is necessary for the openssl call.
  bssl::UniquePtr<GENERAL_NAMES> extension(static_cast<GENERAL_NAMES*>(
      X509_get_ext_d2i(const_cast<X509*>(&cert), NID_subject_alt_name,
                       &out_critical, /*out_idx=*/nullptr)));
  if (extension == nullptr && out_critical == -1) {
    return absl::NotFoundError(
        absl::StrCat("Certificate missing extension ",
                     GetNidShortName(NID_subject_alt_name)));
  }
  if (extension == nullptr && out_critical == -2) {
    return absl::InvalidArgumentError(
        absl::StrCat("Certificate having duplicate ",
                     GetNidShortName(NID_subject_alt_name)));
  }
  if (extension == nullptr) {
    return absl::InternalError(
        absl::StrFormat("Failed to get extension %s: %s.",
                     GetNidShortName(NID_subject_alt_name), GetOpenSslError()));
  }
  if (out_critical != 0 && out_critical != 1) {
    return absl::InternalError(absl::StrCat(
        "X509_get_ext_d2i returns invalid critical values: ", out_critical));
  }
  return extension;
}

// Returns UTF8 string from the given Abstract Syntax Notation One (ASN1) string
absl::StatusOr<std::string> Asn1ToUtf8(ASN1_STRING* asn1) {
  unsigned char* utf8 = nullptr;
  int len = ASN1_STRING_to_UTF8(&utf8, asn1);
  if (len < 0) {
    return absl::InvalidArgumentError("Failed to parse ASN1 string as UTF8.");
  }
  // Free |utf8| when the pointer |owned| goes out of scope.
  bssl::UniquePtr<unsigned char> owned(utf8);
  return std::string(absl::bit_cast<const char*>(utf8), len);
}

absl::StatusOr<bssl::UniquePtr<X509>> X509FromPem(absl::string_view pem) {
  bssl::UniquePtr<BIO> cert_bio(BIO_new_mem_buf(pem.data(), pem.size()));
  bssl::UniquePtr<X509> cert(
      PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr));
  if (cert == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse PEM certificate: ", GetOpenSslError()));
  }
  return cert;
}

absl::StatusOr<bssl::UniquePtr<X509>> FirstX509FromPemChain(
    absl::string_view pem) {
  return X509FromPem(pem);
}
}  // namespace

absl::StatusOr<SubjectAltName> GetSubjectAltName(absl::string_view pem) {
  int out_critical = 0;
  ECCLESIA_ASSIGN_OR_RETURN(bssl::UniquePtr<X509> cert,
                            FirstX509FromPemChain(pem));
  ECCLESIA_ASSIGN_OR_RETURN(bssl::UniquePtr<GENERAL_NAMES> names,
                            GetSubjectAltNameExtension(*cert, out_critical));
  std::vector<std::string> uris;
  std::vector<std::string> fqdns;
  for (int i = 0; i < sk_GENERAL_NAME_num(names.get()); i++) {
    GENERAL_NAME* name = sk_GENERAL_NAME_value(names.get(), i);
    switch (name->type) {
      case GEN_URI: {
        ECCLESIA_ASSIGN_OR_RETURN(
            std::string uri, Asn1ToUtf8(name->d.uniformResourceIdentifier));
        uris.push_back(std::move(uri));
        break;
      }
      case GEN_DNS: {
        ECCLESIA_ASSIGN_OR_RETURN(std::string dns, Asn1ToUtf8(name->d.dNSName));
        fqdns.push_back(std::move(dns));
        break;
      }
      default:
        continue;
    }
  }
  if (uris.size() > 1) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Expected one URI SAN, found %d; URIs are: [%s]",
                        uris.size(), absl::StrJoin(uris, ", ")));
  }
  if (fqdns.size() > 1) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Expected one DNS SAN, found %d; FQDNs are: [%s]",
                        fqdns.size(), absl::StrJoin(fqdns, ", ")));
  }
  SubjectAltName san = {.critical = out_critical == 1};
  if (!uris.empty()) {
    san.spiffe_id = std::move(uris[0]);
  }
  if (!fqdns.empty()) {
    san.fqdn = std::move(fqdns[0]);
  }
  return san;
}

}  // namespace ecclesia
