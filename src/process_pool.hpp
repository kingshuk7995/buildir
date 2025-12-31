#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

namespace exec {

using NodeId = uint32_t;
using Node = std::vector<std::string>;

struct ResultMsg {
  NodeId node_id;
  int32_t exit_code;
};

class ProcessPool {
public:
  explicit ProcessPool(size_t workers);
  ~ProcessPool();

  void start();
  bool can_accept() const;

  void submit(NodeId id, const Node &commands);
  ResultMsg wait_result(); // blocking

  void shutdown(); // safe to call multiple times

private:
  struct Worker {
    pid_t pid = -1;
    int to_child = -1;
    int from_child = -1;
    bool busy = false;
  };

  std::vector<Worker> m_workers;
  bool m_running = false;

  static void worker_loop(int read_fd, int write_fd);
};

} // namespace exec
