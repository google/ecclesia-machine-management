/*
 * Copyright 2023 Google LLC
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

// Presubmit test for TAP to validate new/edited Redpath Query files.
//
// This presubmit validation takes advantage of check_presubmit_build_target
// because it allows access to the changelist and allows TAP to build a binary
// from the client code.
//
// Although this file's BUILD target specifies this as a cc_test, it has a main
// function and a few more special rules. You should not use the standard gunit
// testing framework here, and errors should be reported through the
// WriteResponse function.
//
// More details about check_presubmit_build_target can be found here:
// http://google3/devtools/metadata/presubmit.proto?l=985&rcl=188199871.

#include <stdlib.h>

#include <string>
#include <utility>
#include <vector>

#include "base/init_google.h"
#include "base/logging.h"
#include "devtools/api/finding.pb.h"
#include "devtools/api/source/changelist_presubmit.pb.h"
#include "devtools/api/source/piper/presubmit.pb.h"
#include "devtools/api/source/presubmit_build_target/presubmit_utils.h"
#include "devtools/api/source/presubmit_stubby_service.pb.h"
#include "devtools/api/source/source_file_presubmit.pb.h"
#include "devtools/staticanalysis/findings/api/findings_api.pb.h"
#include "absl/flags/flag.h"
#include "absl/log/globals.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ecclesia/lib/redfish/dellicius/utils/query_validator.h"

using ::devtools::api::Finding;
using ::devtools::api::source::PresubmitRequest;
using ::devtools::api::source::PresubmitResponse;
using ::devtools::api::source::SourceFilePresubmit;
using ::devtools::api::source::presubmit_build_target::GetTestDataPath;
using ::devtools::api::source::presubmit_build_target::ReadRequest;
using ::devtools::api::source::presubmit_build_target::WriteResponse;
using Status = ::devtools::api::source::piper::Presubmit::Status;

namespace ecclesia {

constexpr absl::string_view kDisableValidatorTag =
    "DISABLE_REDPATH_QUERY_VALIDATOR";

static bool IsEditedOrAddedFile(const SourceFilePresubmit& file) {
  return file.operation() == SourceFilePresubmit::OPERATION_ADD ||
          file.operation() == SourceFilePresubmit::OPERATION_EDIT;
}

static std::vector<std::string> GetPathsToCheck(const PresubmitRequest& req) {
  std::vector<std::string> paths;
  for (const auto& file : req.changelist().file()) {
    if (file.has_depot_path() && IsEditedOrAddedFile(file)) {
      paths.push_back(GetTestDataPath(file.depot_path(), false));
    }
  }
  return paths;
}

static void RunPresubmitGuardRails(absl::Span<const std::string> paths,
                                   PresubmitResponse& resp) {
  RedPathQueryValidator validator;
  for (absl::string_view path : paths) {
    LOG(INFO) << "Running guardrails on redpath query file: " << path << '\n';
    absl::Status validator_run_status = validator.ValidateQueryFile(path);
    if (!validator_run_status.ok()) {
      LOG(ERROR) << validator_run_status;
    }
  }
  resp.set_succeeded(true);
  // Add warnings to response. Add each warning as a Finding member in the
  // PresubmitResponse. Downgrade the failure status to warning if there
  // are no errors.
  if (!validator.GetWarnings().empty()) {
    resp.set_succeeded(false);
    for (const RedPathQueryValidator::Issue& warning :
         validator.GetWarnings()) {
      Finding finding;
      finding.set_actionable(true);
      finding.mutable_location()->set_path(warning.path);
      finding.set_message(absl::StrCat(
          RedPathQueryValidator::Issue::GetDescriptor(warning.type),
          " Warning: ", warning.message));
      *resp.add_finding() = std::move(finding);
    }
  }
  // Return early if no error level issues.
  if (validator.GetErrors().empty()) {
    resp.set_failure_message(absl::StrCat(
        "Warnings raised for one or more redpath queries. These can be "
        "suppressed with the tag ",
        kDisableValidatorTag));
    resp.set_downgrade_failure_status(Status::WARNING);
    return;
  }
  // Add redpath query validation errors to response.
  resp.set_succeeded(false);
  resp.set_failure_message(
      "One or more redpath query validations failed. Please fix all query "
      "errors before submitting.");
  for (const RedPathQueryValidator::Issue& error : validator.GetErrors()) {
    Finding error_finding;
    error_finding.set_actionable(true);
    error_finding.mutable_location()->set_path(error.path);
    error_finding.set_message(
        absl::StrCat(RedPathQueryValidator::Issue::GetDescriptor(error.type),
                     " Error: ", error.message));
    resp.set_downgrade_failure_status(Status::ERROR);
    *resp.add_finding() = std::move(error_finding);
  }
}
}  // namespace ecclesia

// Flag that allows users to run the validator manually and pass in paths to
// redpath queries, relative to google3.
// Example run syntax:
//   blaze-bin/ecclesia/lib/redfish/dellicius/presubmit/query_presubmit_validation
//    --extra_paths <path/to/first query>, <path/to/second/query>
ABSL_FLAG(std::vector<std::string>, extra_paths, std::vector<std::string>{},
          "comma-separated list of paths to check.");

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, false);
  // Set the error threshold to fatal errors only, since the presubmit check
  // relies on reading the PresubmitResponse from stdout, logs will interfere.
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kFatal);
  PresubmitResponse resp;
  PresubmitRequest req = ReadRequest();

  // Return early if validator is disabled or no changelist present.
  if (absl::StrContains(req.changelist().description().text(),
                        ecclesia::kDisableValidatorTag)) {
    resp.set_succeeded(true);
    WriteResponse(resp);
    return EXIT_SUCCESS;
  }
  VLOG(1) << "PresubmitRequest: " << req << '\n';
  if (!req.has_changelist()) {
    resp.set_succeeded(false);
    resp.set_failure_message("PresubmitResponse had no changelist.");
    WriteResponse(resp);
    return EXIT_SUCCESS;
  }

  std::vector<std::string> paths = ecclesia::GetPathsToCheck(req);
  for (const std::string& path : absl::GetFlag(FLAGS_extra_paths)) {
    paths.push_back(path);
  }
  ecclesia::RunPresubmitGuardRails(paths, resp);

  WriteResponse(resp);
  return EXIT_SUCCESS;
}
