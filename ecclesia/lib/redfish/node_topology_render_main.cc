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

// Create a dot graph render of a Node Topology generated from a Redfish backend

#include <algorithm>
#include <deque>
#include <iostream>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ecclesia/lib/http/cred.pb.h"
#include "ecclesia/lib/http/curl_client.h"
#include "ecclesia/lib/redfish/interface.h"
#include "ecclesia/lib/redfish/node_topology.h"
#include "ecclesia/lib/redfish/topology.h"
#include "ecclesia/lib/redfish/transport/cache.h"
#include "ecclesia/lib/redfish/transport/http.h"
#include "ecclesia/lib/redfish/transport/http_redfish_intf.h"

ABSL_FLAG(std::string, hostname_port, "",
          "TCP endpoint to connect with both hostname and port separated by a "
          "colon i.e. \"localhost:8000\"");
ABSL_FLAG(std::string, unix_domain_socket, "", "UDS endpoint to connect to");

namespace ecclesia {
namespace {

int RealMain(int argc, char* argv[]) {
  absl::InitializeLog();
  absl::ParseCommandLine(argc, argv);

  std::string hostname_port = absl::GetFlag(FLAGS_hostname_port),
              unix_domain_socket = absl::GetFlag(FLAGS_unix_domain_socket);

  // Either TCP or UDS must provided
  if (hostname_port.empty() == unix_domain_socket.empty()) {
    std::cout << "USAGE: " << argv[0]
              << " (--hostname_port=\"{hostname:port}\") XOR "
                 "(--unix_domain_socket=\"{unix_domain.socket}\")"
              << "\n";
    return -1;
  }

  std::unique_ptr<CurlHttpClient> curl_http_client =
      std::make_unique<CurlHttpClient>(LibCurlProxy::CreateInstance(),
                                       HttpCredential());
  std::unique_ptr<HttpRedfishTransport> transport =
      hostname_port.empty()
          ? HttpRedfishTransport::MakeUds(std::move(curl_http_client),
                                          unix_domain_socket)
          : HttpRedfishTransport::MakeNetwork(std::move(curl_http_client),
                                              hostname_port);
  std::unique_ptr<RedfishCachedGetterInterface> cache =
      std::make_unique<NullCache>(transport.get());
  std::unique_ptr<RedfishInterface> intf = NewHttpInterface(
      std::move(transport), std::move(cache), RedfishInterface::kTrusted);

  if (!intf || !intf->GetRoot().status().ok()) {
    std::cerr << "Failed to connect to backend" << std::endl;
    return -1;
  }

  NodeTopology topology = CreateTopologyFromRedfish(intf.get());

  std::vector<std::string> lines;
  lines.push_back("// Start of the digraph");
  lines.push_back("digraph {");
  lines.push_back("  graph [splines=ortho, nodesep=0.25]");
  lines.push_back("  rankdir = LR;");
  lines.push_back("  node [shape=box];");
  lines.push_back("  edge [arrowsize=0.75];");

  absl::flat_hash_map<std::string, uint32_t> devpath_to_counter;
  absl::flat_hash_map<Node*, std::string> node_to_secondary_devpath;
  for (auto& node : topology.nodes) {
    std::string devpath = node->local_devpath;
    auto [it, inserted] = devpath_to_counter.try_emplace(devpath, 0);
    if (!inserted) {
      // Need to create the node as a secondary node
      lines.push_back("  // Duplicate devpath found");
      uint32_t current_counter = ++it->second;
      absl::StrAppend(&devpath, "_dupe_", current_counter);
      node_to_secondary_devpath[node.get()] = devpath;
    }

    lines.push_back(absl::StrFormat(
        "  \"%s\" [tooltip=\"%s\", label=<<b>%s</b><br/><i>%s</i>>]", devpath,
        absl::StrJoin(node->associated_uris, "\\n"), devpath, node->name));
  }

  auto it = topology.devpath_to_node_map.find("/phys");
  if (it == topology.devpath_to_node_map.end()) {
    lines.push_back("// No root node found");
  } else {
    std::deque<Node*> queue;
    queue.push_back(it->second);

    while (!queue.empty()) {
      auto* current_node = queue.front();
      queue.pop_front();

      auto children = topology.node_to_children.find(current_node);
      if (children != topology.node_to_children.end()) {
        // Check if the current node has a duplicated devpath
        auto current_node_local_devpath = current_node->local_devpath;
        if (auto current_node_it = node_to_secondary_devpath.find(current_node);
            current_node_it != node_to_secondary_devpath.end()) {
          current_node_local_devpath = current_node_it->second;
        }

        for (auto* child : children->second) {
          // Check if the child node has a duplicated devpath
          auto child_local_devpath = child->local_devpath;
          if (auto child_it = node_to_secondary_devpath.find(child);
              child_it != node_to_secondary_devpath.end()) {
            child_local_devpath = child_it->second;
          }

          lines.push_back(absl::StrFormat("  \"%s\" -> \"%s\"",
                                          current_node_local_devpath,
                                          child_local_devpath));

          queue.push_back(child);
        }
      }
    }
  }

  lines.push_back("}");

  std::cout << absl::StrJoin(lines, "\n") << "\n";

  return 0;
}

}  // namespace
}  // namespace ecclesia

int main(int argc, char* argv[]) { return ecclesia::RealMain(argc, argv); }
