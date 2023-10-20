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

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/span.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/redfish/dellicius/query/builder.h"
#include "ecclesia/lib/redfish/dellicius/query/query.pb.h"
#include "ecclesia/lib/status/macros.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(std::string, name, "",
          "name of the .cc and .h files to be generated");

ABSL_FLAG(std::string, namespace, "",
          "namespace for the functions to be generated");

ABSL_FLAG(std::string, output_dir, "",
          "directory where the output files should be written");

ABSL_FLAG(std::vector<std::string>, query_files, {},
          "list of query textproto files");

ABSL_FLAG(std::vector<std::string>, query_rules, {},
          "list of query rule textproto files");

namespace platforms_redfish {

namespace {

constexpr absl::string_view kFileTemplate = R"(
#include "absl/container/flat_hash_set.h"
#include "ecclesia/lib/file/cc_embed_interface.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/time/clock.h"

namespace $0 {

$1
}  // namespace $0

)";

constexpr absl::string_view kHeaderFileTemplate = R"(
#include "absl/container/flat_hash_set.h"
#include "ecclesia/lib/redfish/dellicius/engine/query_engine.h"
#include "ecclesia/lib/time/clock.h"

namespace $0 {

absl::flat_hash_set<std::string> Get$1QueryIds();

ecclesia::QueryContext Get$1QueryContext(
    const ecclesia::Clock *clock = ecclesia::Clock::RealClock());

}  // namespace $0
)";

constexpr absl::string_view kSourceFileTemplate = R"(
absl::flat_hash_set<std::string> Get$0QueryIds() {
  return {"$2"};
}

ecclesia::QueryContext Get$0QueryContext(const ecclesia::Clock *clock) {
  return ecclesia::QueryContext{.query_files = k$0Query,
                                $1
                                .clock = clock};
}
)";

using FilenameContentMap = absl::flat_hash_map<std::string, std::string>;

absl::StatusOr<FilenameContentMap> ReadFileContents(
    absl::Span<const std::string> files) {
  FilenameContentMap contents;
  std::string buffer(1024, '\0');
  for (absl::string_view filename : files) {
    // Open up the input file.
    std::fstream in_f(std::string(filename),
                      std::fstream::binary | std::fstream::in);
    if (!in_f.is_open()) {
      return absl::InternalError(absl::StrCat("unable to open ", filename));
    }
    std::string content;
    while (!in_f.eof()) {
      in_f.read(&buffer[0], static_cast<int64_t>(buffer.size()));
      absl::string_view used_buffer(buffer.data(), in_f.gcount());
      absl::StrAppend(&content, used_buffer);
    }
    contents.insert({std::string(ecclesia::GetBasename(filename)), content});
  }
  return contents;
}

absl::StatusOr<absl::flat_hash_set<std::string>> GetQueryIds(
    const FilenameContentMap &filename_contents) {
  absl::flat_hash_set<std::string> query_ids;
  for (const auto &[filename, contents] : filename_contents) {
    ecclesia::DelliciusQuery query;
    if (!google::protobuf::TextFormat::ParseFromString(contents, &query)) {
      return absl::InternalError(
          absl::StrCat("Unable to parse query file: ", filename));
    }
    const auto [it, inserted] = query_ids.insert(query.query_id());
    if (!inserted) {
      return absl::InternalError(
          absl::StrCat("Duplicate query id: ", query.query_id()));
    }
  }
  if (query_ids.empty()) {
    return absl::InternalError("No query ids found");
  }
  return query_ids;
}

std::string GetFileContentsStr(
    absl::string_view name, absl::string_view suffix,
    const absl::flat_hash_map<std::string, std::string> &filename_contents) {
  std::string content =
      absl::StrFormat("ecclesia::EmbeddedFileArray<%d> k%s%s = {{",
                      filename_contents.size(), name, suffix);
  for (const auto &[filename, contents] : filename_contents) {
    absl::StrAppend(&content, "{\"", absl::CEscape(filename),
                    "\", absl::string_view(\"", absl::CEscape(contents), "\", ",
                    contents.size(), ")},\n");
  }
  absl::StrAppend(&content, "}};\n\n");
  return content;
}

}  // namespace

class QuerySpecBuilder : public FileBuilderBase {
 public:
  QuerySpecBuilder(absl::string_view name, absl::string_view ns,
                   absl::string_view output_dir,
                   std::vector<std::string> query_files,
                   std::vector<std::string> query_rules)
      : FileBuilderBase(name, output_dir),
        namespace_(ns),
        query_files_(std::move(query_files)),
        query_rules_(std::move(query_rules)) {}

  absl::Status WriteFiles() const override {
    ECCLESIA_RETURN_IF_ERROR(WriteHeaderFile(
        absl::Substitute(kHeaderFileTemplate, namespace_, name())));
    return CreateSourceFile();
  }

 private:
  absl::Status CreateSourceFile() const {
    // Read query and query rule files first
    ECCLESIA_ASSIGN_OR_RETURN(FilenameContentMap query_file_contents,
                              ReadFileContents(query_files_));
    ECCLESIA_ASSIGN_OR_RETURN(FilenameContentMap query_rule_contents,
                              ReadFileContents(query_rules_));

    std::string context_query_rule;

    std::string content =
        GetFileContentsStr(name(), "Query", query_file_contents);
    if (!query_rule_contents.empty()) {
      absl::StrAppend(&content, GetFileContentsStr(name(), "QueryRule",
                                                   query_rule_contents));
      context_query_rule =
          absl::StrFormat(".query_rules = k%sQueryRule,", name());
    }

    ECCLESIA_ASSIGN_OR_RETURN(absl::flat_hash_set<std::string> query_ids,
                              GetQueryIds(query_file_contents));
    std::string query_ids_str = absl::StrJoin(query_ids, "\" , \"");

    absl::StrAppend(&content,
                    absl::Substitute(kSourceFileTemplate, name(),
                                     context_query_rule, query_ids_str),
                    "\n");

    return WriteSourceFile(
        absl::Substitute(kFileTemplate, namespace_, content));
  }

  std::string namespace_;
  std::vector<std::string> query_files_;
  std::vector<std::string> query_rules_;
};

static absl::Status GenerateQuerySpec() {
  if (absl::GetFlag(FLAGS_name).empty()) {
    return absl::FailedPreconditionError("name must be specified");
  }
  if (absl::GetFlag(FLAGS_namespace).empty()) {
    return absl::FailedPreconditionError("namespace must be specified");
  }
  if (absl::GetFlag(FLAGS_query_files).empty()) {
    return absl::FailedPreconditionError("query_files must be specified");
  }
  if (absl::GetFlag(FLAGS_output_dir).empty()) {
    return absl::FailedPreconditionError("output_dir must be specified");
  }

  std::unique_ptr<FileBuilderBase> builder = std::make_unique<QuerySpecBuilder>(
      absl::GetFlag(FLAGS_name), absl::GetFlag(FLAGS_namespace),
      absl::GetFlag(FLAGS_output_dir), absl::GetFlag(FLAGS_query_files),
      absl::GetFlag(FLAGS_query_rules));

  return builder->WriteFiles();
}

}  // namespace platforms_redfish

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  absl::Status status = platforms_redfish::GenerateQuerySpec();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to generate query spec: " << status.message();
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
