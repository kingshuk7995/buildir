#pragma once

#include <cstdint>
#include <fstream>
#include <numeric>
#include <optional>
#include <parse.hpp>
#include <process_pool.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utils.hpp>
#include <vector>

namespace exec {

constexpr std::string default_cmd = "_default";
constexpr uint32_t default_procs = 2;
using Node = std::vector<std::string>;
using NodeId = uint32_t;

class Graph {
public:
  static constexpr NodeId npos = std::numeric_limits<NodeId>::max();
  static constexpr std::string serialize_file = ".graph_cache";
  static constexpr uint32_t GRAPH_SERDE_VERSION = 1;
  Graph() = delete;

  static Graph build(const parse::Result &parsed);

  inline Ref<const Node> get_command_ref(NodeId node_id) const noexcept {
    return Ref<const Node>{&m_node_store[node_id]};
  }

  inline std::span<const NodeId> get_child_ids(NodeId node_id) const noexcept {
    const auto &v = m_adjgraph[node_id];
    return {v.data(), v.size()};
  }

  inline std::span<const NodeId> get_parent_ids(NodeId node_id) const noexcept {
    const auto &v = m_reverse_adj[node_id];
    return {v.data(), v.size()};
  }

  inline NodeId get_id(const std::string &name) const noexcept {
    auto id = this->m_id_map.find(name);
    if (id == this->m_id_map.end()) {
      return Graph::npos;
    } else {
      return id->second;
    }
  }

  inline Ref<const std::string> get_name_ref(NodeId id) const noexcept {
    return Ref<const std::string>(&(m_names[id]));
  }

  inline std::size_t size() const noexcept { return m_node_store.size(); }

  inline bool is_phony(const NodeId id) const noexcept {
    return (m_phony.find(id) != m_phony.end());
  }

  void serialize() const;
  static Graph deserialize();

private:
  explicit Graph(std::vector<Node> &&node_store,
                 std::vector<std::vector<NodeId>> &&adjgraph,
                 std::vector<std::vector<NodeId>> &&revgraph,
                 std::unordered_map<std::string, uint32_t> &&id_map,
                 std::unordered_set<NodeId> &&phony,
                 std::vector<std::string> &&names)
      : m_node_store(std::move(node_store)), m_adjgraph(std::move(adjgraph)),
        m_reverse_adj(std::move(revgraph)), m_id_map(std::move(id_map)),
        m_phony(std::move(phony)), m_names(std::move(names)) {}

  const std::vector<Node> m_node_store;
  const std::vector<std::vector<NodeId>> m_adjgraph;
  const std::vector<std::vector<NodeId>> m_reverse_adj;
  const std::unordered_map<std::string, uint32_t> m_id_map;
  const std::unordered_set<NodeId> m_phony;
  const std::vector<std::string> m_names;
};

class Scheduler {
public:
  Scheduler(uint32_t n_workers) : pool(n_workers) {}

  inline void start_pool() { pool.start(); }
  void run(const Graph &graph, const std::string &start);

private:
  inline void execute_node(NodeId id, const Node &node) {
    pool.submit(id, node);
  }

  ProcessPool pool;
};

} // namespace exec
