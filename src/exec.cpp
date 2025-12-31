#include <exec.hpp>
#include <filesystem>
#include <format>
#include <queue>
#include <serde_utils.hpp>
#include <unordered_map>
#include <utils.hpp>

namespace exec {

Graph Graph::build(const parse::Result &parsed) {
  std::unordered_map<std::string, NodeId> id_map;
  id_map.reserve(parsed.rules.size());
  std::vector<std::string> names;
  names.reserve(parsed.rules.size());

  for (NodeId i = 0; i < parsed.rules.size(); ++i) {
    auto [it, ok] = id_map.emplace(parsed.rules[i].name, i);
    names.push_back(parsed.rules[i].name);
    if (!ok) {
      fatal("duplicate rule name");
    }
  }

  const NodeId n = static_cast<NodeId>(parsed.rules.size());

  std::vector<std::vector<NodeId>> adj(n), rev(n);
  std::vector<Node> nodes;
  nodes.reserve(n);

  for (const auto &rule : parsed.rules) {
    const NodeId child = id_map.at(rule.name);
    nodes.push_back(rule.commands);

    for (const auto &dep : rule.deps) {
      auto it = id_map.find(dep);
      if (it == id_map.end())
        fatal("dependency not found");

      const NodeId parent = it->second;
      adj[parent].push_back(child);
      rev[child].push_back(parent);
    }
  }

  std::unordered_set<NodeId> phoneyset;

  for (const auto &p : parsed.phony) {
    auto fnd = id_map.find(p);
    if (fnd == id_map.end()) {
      fatal("phony command not found in build");
    }
    phoneyset.insert(fnd->second);
  }

  return Graph(std::move(nodes), std::move(adj), std::move(rev),
               std::move(id_map), std::move(phoneyset), std::move(names));
}

void Graph::serialize() const {
  std::vector<std::byte> bytestream;
  bytestream.reserve(4096);

  // version
  auto ver = serde::serialize_value<uint32_t>(GRAPH_SERDE_VERSION);
  bytestream.insert(bytestream.end(), ver.begin(), ver.end());

  serde::serialize_vec(this->m_node_store, bytestream);
  serde::serialize_vec(this->m_adjgraph, bytestream);
  serde::serialize_vec(this->m_reverse_adj, bytestream);
  serde::serialize_map(this->m_id_map, bytestream);
  serde::serialize_vec(std::vector(this->m_phony.begin(), this->m_phony.end()),
                       bytestream);
  serde::serialize_vec(this->m_names, bytestream);

  std::ofstream out(Graph::serialize_file, std::ios::binary | std::ios::trunc);
  if (!out) {
    fatal("failed to open graph cache for writing");
  }

  out.write(reinterpret_cast<const char *>(bytestream.data()),
            static_cast<std::streamsize>(bytestream.size()));
  if (!out) {
    fatal("failed to write graph cache");
  }
}

Graph Graph::deserialize() {
  namespace fs = std::filesystem;

  if (!fs::exists(Graph::serialize_file)) {
    fatal("graph cache not found");
  }

  const auto filesize = fs::file_size(Graph::serialize_file);
  if (filesize == 0) {
    fatal("graph cache is empty");
  }

  std::ifstream in(Graph::serialize_file, std::ios::binary);
  if (!in) {
    fatal("failed to open graph cache for reading");
  }

  std::vector<std::byte> buffer(filesize);
  in.read(reinterpret_cast<char *>(buffer.data()),
          static_cast<std::streamsize>(buffer.size()));
  if (!in) {
    fatal("failed to read graph cache");
  }

  const std::byte *ptr = buffer.data();

  // ---- version check ----
  uint32_t version = serde::deserialize_value<uint32_t>(ptr);
  if (version != GRAPH_SERDE_VERSION) {
    fatal("graph cache version mismatch");
  }

  // ---- payload ----
  auto node_store = serde::deserialize_vec<std::vector<Node>>(ptr);
  auto adjgraph = serde::deserialize_vec<std::vector<std::vector<NodeId>>>(ptr);
  auto reverse_adj =
      serde::deserialize_vec<std::vector<std::vector<NodeId>>>(ptr);
  auto id_map =
      serde::deserialize_map<std::unordered_map<std::string, NodeId>>(ptr);
  auto phony = serde::deserialize_vec<std::vector<NodeId>>(ptr);
  auto names = serde::deserialize_vec<std::vector<std::string>>(ptr);

  // checks
  const size_t n = node_store.size();
  if (adjgraph.size() != n || reverse_adj.size() != n || names.size() != n ||
      id_map.size() != n) {
    fatal("graph cache corrupted: size mismatch");
  }

  if (ptr != buffer.data() + buffer.size()) {
    fatal("graph cache corrupted: trailing bytes");
  }

  return Graph(std::move(node_store), std::move(adjgraph),
               std::move(reverse_adj), std::move(id_map),
               std::unordered_set(phony.begin(), phony.end()),
               std::move(names));
}

void Scheduler::run(const Graph &graph, const std::string &start) {
  const NodeId N = static_cast<uint32_t>(graph.size());

  const NodeId start_id = graph.get_id(start);
  if (start_id == Graph::npos) {
    if (start == exec::default_cmd) {
      std::cerr << "fallback to default command: " << exec::default_cmd << '\n';
    }
    fatal(std::format("block: {} not available", start).c_str());
  }

  // 1. Compute required subgraph (reverse DFS)
  std::vector<uint8_t> needed(N, false);
  {
    std::vector<NodeId> st;
    st.reserve(N / 4);

    st.push_back(start_id);
    needed[start_id] = true;

    while (!st.empty()) {
      NodeId u = st.back();
      st.pop_back();

      for (NodeId p : graph.get_parent_ids(u)) {
        if (!needed[p]) {
          needed[p] = true;
          st.push_back(p);
        }
      }
    }
  }

  // 2. Compute indegrees (restricted to needed subgraph)
  std::vector<uint32_t> indegree(N, 0);
  std::queue<NodeId> ready;

  for (NodeId u = 0; u < N; ++u) {
    if (!needed[u])
      continue;

    for (NodeId v : graph.get_child_ids(u)) {
      if (needed[v]) {
        indegree[v]++;
      }
    }
  }

  // 3. Initialize ready queue
  for (NodeId i = 0; i < N; ++i) {
    if (needed[i] && indegree[i] == 0) {
      ready.push(i);
    }
  }

  // 4. Helper: should_execute(u)
  auto should_execute = [&](NodeId u) -> bool {
    if (graph.is_phony(u)) {
      return true;
    }

    const std::string &target = *graph.get_name_ref(u);

    // target does not exist → must execute
    if (!std::filesystem::exists(target)) {
      return true;
    }

    // any dependency newer → must execute
    for (NodeId p : graph.get_parent_ids(u)) {
      const std::string &dep = *graph.get_name_ref(p);
      if (is_newer(dep, target)) {
        return true;
      }
    }

    return false; // up-to-date
  };

  // 5. Start process pool (prefork already done)
  // start_pool();   // assuming pool already initialized

  uint32_t running = 0;

  // 6. Main scheduling loop
  while (!ready.empty() || running > 0) {

    // Dispatch while capacity available
    while (!ready.empty() && pool.can_accept()) {
      NodeId u = ready.front();
      ready.pop();

      if (should_execute(u)) {
        const Node &node = *graph.get_command_ref(u);
        pool.submit(u, node);
        running++;
      } else {
        // skipped node → instant success
        for (NodeId v : graph.get_child_ids(u)) {
          if (needed[v] && --indegree[v] == 0) {
            ready.push(v);
          }
        }
      }
    }

    // If nothing running, continue draining ready
    if (running == 0)
      continue;

    // Wait for one task to finish
    auto res = pool.wait_result();
    running--;

    if (res.exit_code != 0) {
      pool.shutdown();
      fatal("command failed");
    }

    // Propagate completion
    for (NodeId v : graph.get_child_ids(res.node_id)) {
      if (needed[v] && --indegree[v] == 0) {
        ready.push(v);
      }
    }
  }

  pool.shutdown();

  // Cycle detection (needed subgraph only)
  for (NodeId i = 0; i < N; ++i) {
    if (needed[i] && indegree[i] != 0) {
      fatal("cycle detected in dependency graph");
    }
  }
}

} // namespace exec
