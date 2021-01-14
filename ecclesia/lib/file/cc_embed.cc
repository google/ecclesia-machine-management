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

// This is a command-line utility that will generate a C++ header and source
// file embedding a collection of files as raw bytes.
//
// The utility takes a list of input files. The files specified will be read in
// and encoded into an EmbeddedFileArray data structure under their name,
// possibly with some manipulations via the strip_prefixes and flatten
// arguments.
//
// The utility also requires an output name and directory, a namespace, and a
// variable name as flag parameters. It will then generate a .h and .cc file
// with the given source name that defines a single variable of type
// EmbeddedFileArray containing data from all of the given files.

#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/strings/substitute.h"
#include "ecclesia/lib/file/path.h"
#include "ecclesia/lib/logging/globals.h"
#include "ecclesia/lib/logging/logging.h"

// Flags that control the output files.
ABSL_FLAG(std::string, output_name, "",
          "Name of the .cc and .h files that will be generated.");
ABSL_FLAG(std::string, output_dir, ".",
          "Directory where the output files should be written.");

// Flags that specify the symbols embedded into the output file.
ABSL_FLAG(
    std::string, namespace, "",
    "The namespace the generated array variable will be inserted into. "
    "Multiple levels of nesting can be specified by using :: in the name.");
ABSL_FLAG(std::string, variable_name, "",
          "The name of the EmbeddedFileArray variable.");

// Flags that manipulate the paths of the output names.
ABSL_FLAG(std::vector<std::string>, strip_prefixes, {},
          "A list of prefix directories to strip off of each filename. If "
          "multiple prefixes are specified then they'll be stripped off in the "
          "order in which they were specified.");
ABSL_FLAG(
    bool, flatten, false,
    "If specified then all directories will be stripped from output names, "
    "effectively flattening the names into a single directory of files.");

namespace ecclesia {
namespace {

// Template for defining the output header, as an absl::Substitute template.
// Expects the parameters:
//   0 - the namespace
//   1 - the number of embedded files
//   2 - the variable name
constexpr absl::string_view kHeaderTemplate =
    R"(#include "ecclesia/lib/file/cc_embed_interface.h"
namespace $0 {
extern ::ecclesia::EmbeddedFileArray<$1> $2;
}  // namespace $0
)";

// Template for defining the output source prefix, as an absl::Substitute
// template. Expects the parameters:
//   0 - the namespace
//   1 - the number of embedded files
//   2 - the variable name
// The prefix ends at the opening brace of the terminating function. Thus the
// actual contents of the array need to be defined, and then the file closed out
// with the suffix.
constexpr absl::string_view kSourcePrefixTemplate =
    R"(#include "ecclesia/lib/file/cc_embed_interface.h"
namespace $0 {
extern ::ecclesia::EmbeddedFileArray<$1> $2;
::ecclesia::EmbeddedFileArray<$1> $2 = {{
)";

// Template for defining the output source suffix, as an absl::Substitute
// template. Expects the parameters:
//   0 - the namespace
// The suffix will close out the array declaration and finish off the file.
constexpr absl::string_view kSourceSuffixTemplate =
    R"(}};
}  // namespace $0
)";

// Given the path to an input file, use the strip_prefixes and flatten to
// transform it into the name it should be embedded under.
absl::string_view NameFromPath(absl::string_view path) {
  // If --flatten is specified then we can just strip the path down to the
  // basename and return that, no need to work through all the directory
  // manipulation from --strip_prefixes.
  if (absl::GetFlag(FLAGS_flatten)) {
    return GetBasename(path);
  }
  // Otherwise we need to pull off all of the prefixes, one-by-one. We append a
  // slash to each prefix to ensure that we're only stripping off a directory
  // prefix and not an arbitrary string.
  for (const std::string &prefix : absl::GetFlag(FLAGS_strip_prefixes)) {
    absl::ConsumePrefix(&path, prefix + "/");
  }
  return path;
}

}  // namespace

int RealMain(int argc, char *argv[]) {
  std::vector<char *> args = absl::ParseCommandLine(argc, argv);

  // Make sure all of the required flags were specified.
  if (absl::GetFlag(FLAGS_output_name).empty()) {
    FatalLog() << "source_name was not specified";
  }
  if (absl::GetFlag(FLAGS_namespace).empty()) {
    FatalLog() << "namespace was not specified";
  }
  if (absl::GetFlag(FLAGS_variable_name).empty()) {
    FatalLog() << "variable_name was not specified";
  }

  // Strip off the leading entry of args. The rest of the arguments are now the
  // filenames to be embedded.
  args.erase(args.begin());

  // Write out the header file. It contains nothing but an extern declaration
  // of the big array of data.
  //
  // We don't bother putting an include guard in the header. Having multiple
  // copies of an extern declaration in the code doesn't cause any problems
  // and so there isn't a ton of benefit to saving duplicate declarations.
  // Adding an include guard would require adding yet another parameter to
  // this binary and the caller would have to come up with a globally unique
  // guard symbol. Omitting it simplifies the usage.
  {
    std::string header_path =
        JoinFilePaths(absl::GetFlag(FLAGS_output_dir),
                      absl::StrCat(absl::GetFlag(FLAGS_output_name), ".h"));
    std::fstream out_f(header_path, out_f.binary | out_f.trunc | out_f.out);
    if (!out_f.is_open()) FatalLog() << "unable to open " << header_path;
    out_f << absl::Substitute(kHeaderTemplate, absl::GetFlag(FLAGS_namespace),
                              args.size(), absl::GetFlag(FLAGS_variable_name));
  }

  // Write out the source file. The implementation we use is fairly simple.
  // The source file just defines a single variable which is a std::array of
  // string_view pairs. We fill it with data by reading in all the individual
  // files and turning them into huge string literals.
  {
    std::string source_path =
        JoinFilePaths(absl::GetFlag(FLAGS_output_dir),
                      absl::StrCat(absl::GetFlag(FLAGS_output_name), ".cc"));
    std::fstream out_f(source_path, out_f.binary | out_f.trunc | out_f.out);
    if (!out_f.is_open()) FatalLog() << "unable to open " << source_path;
    out_f << absl::Substitute(kSourcePrefixTemplate,
                              absl::GetFlag(FLAGS_namespace), args.size(),
                              absl::GetFlag(FLAGS_variable_name));
    // Track the names we've added.
    absl::flat_hash_set<absl::string_view> input_names;
    // Write out an entry for each file in args.
    std::string buffer(1024, '\0');
    for (const char *input_path : args) {
      // Open up the input file.
      std::fstream in_f(input_path, in_f.binary | in_f.in);
      if (!in_f.is_open()) FatalLog() << "unable to open " << input_path;
      // Make sure there's no duplicate name.
      absl::string_view input_name = NameFromPath(input_path);
      if (!input_names.insert(input_name).second) {
        FatalLog() << "multiple entries named: " << input_name;
      }
      // Start the EmbeddedFile entry and write out the input name. Note that
      // we have to wrap all the data literals in an explicitly sized
      // string_view constructor, or else the compiler will treat it as a
      // NUL-terminated string causing any files with NUL in them will get
      // truncated.
      out_f << "{\"" << absl::CEscape(input_name) << "\", absl::string_view(\n";
      // Read in the contents in buffer-sized chunks and copy them into the
      // output file with appropriate escaping. We put one chunk per line
      // rather than generating one huge line.
      size_t data_size = 0;
      while (!in_f.eof()) {
        in_f.read(&buffer[0], buffer.size());
        absl::string_view used_buffer(buffer.data(), in_f.gcount());
        out_f << "\"" << absl::CEscape(used_buffer) << "\"\n";
        data_size += used_buffer.size();
      }
      // Close out the string_view constructor and EmbeddedFile entry.
      out_f << ", " << data_size << ")},\n";
    }
    out_f << absl::Substitute(kSourceSuffixTemplate,
                              absl::GetFlag(FLAGS_namespace));
  }

  return 0;
}

}  // namespace ecclesia

int main(int argc, char *argv[]) { ecclesia::RealMain(argc, argv); }
