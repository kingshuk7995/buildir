#include "process_pool.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

namespace exec {

// IPC messages

struct TaskMsg {
  NodeId node_id;
  uint32_t cmd_count; // 0 => shutdown
};

// Worker process code

void ProcessPool::worker_loop(int read_fd, int write_fd) {
  while (true) {
    TaskMsg msg;
    ssize_t r = read(read_fd, &msg, sizeof(msg));
    if (r <= 0)
      break;

    if (msg.cmd_count == 0)
      break; // shutdown

    int rc = 0;

    for (uint32_t i = 0; i < msg.cmd_count; ++i) {
      uint32_t len;
      read(read_fd, &len, sizeof(len));

      std::string cmd(len, '\0');
      read(read_fd, cmd.data(), len);

      rc = std::system(cmd.c_str());
      if (rc != 0)
        break;
    }

    ResultMsg res{msg.node_id, rc};
    write(write_fd, &res, sizeof(res));
  }

  _exit(0);
}

// ProcessPool impl

ProcessPool::ProcessPool(size_t workers) : m_workers(workers) {}

ProcessPool::~ProcessPool() { shutdown(); }

void ProcessPool::start() {
  if (m_running)
    return;

  for (auto &w : m_workers) {
    int p2c[2], c2p[2];
    pipe(p2c);
    pipe(c2p);

    pid_t pid = fork();
    if (pid == 0) {
      // child
      close(p2c[1]);
      close(c2p[0]);

      // reset signals to default
      signal(SIGINT, SIG_DFL);
      signal(SIGTERM, SIG_DFL);

      worker_loop(p2c[0], c2p[1]);
    }

    // parent
    close(p2c[0]);
    close(c2p[1]);

    w.pid = pid;
    w.to_child = p2c[1];
    w.from_child = c2p[0];
    w.busy = false;
  }

  m_running = true;
}

bool ProcessPool::can_accept() const {
  return std::any_of(m_workers.begin(), m_workers.end(),
                     [](const Worker &w) { return !w.busy; });
}

void ProcessPool::submit(NodeId id, const Node &commands) {
  for (auto &w : m_workers) {
    if (!w.busy) {
      TaskMsg msg{id, static_cast<uint32_t>(commands.size())};
      write(w.to_child, &msg, sizeof(msg));

      for (const auto &cmd : commands) {
        uint32_t len = cmd.size();
        write(w.to_child, &len, sizeof(len));
        write(w.to_child, cmd.data(), len);
      }

      w.busy = true;
      return;
    }
  }

  std::cerr << "ProcessPool: no free worker\n";
  std::abort();
}

ResultMsg ProcessPool::wait_result() {
  fd_set set;
  FD_ZERO(&set);

  int maxfd = -1;
  for (const auto &w : m_workers) {
    if (w.busy) {
      FD_SET(w.from_child, &set);
      maxfd = std::max(maxfd, w.from_child);
    }
  }

  select(maxfd + 1, &set, nullptr, nullptr, nullptr);

  for (auto &w : m_workers) {
    if (w.busy && FD_ISSET(w.from_child, &set)) {
      ResultMsg res;
      read(w.from_child, &res, sizeof(res));
      w.busy = false;
      return res;
    }
  }

  std::abort();
}

void ProcessPool::shutdown() {
  if (!m_running)
    return;

  // tell workers to exit
  for (auto &w : m_workers) {
    if (w.pid > 0) {
      TaskMsg msg{0, 0};
      write(w.to_child, &msg, sizeof(msg));
    }
  }

  // wait & reap
  for (auto &w : m_workers) {
    if (w.pid > 0) {
      kill(w.pid, SIGTERM);
      waitpid(w.pid, nullptr, 0);
      close(w.to_child);
      close(w.from_child);
      w.pid = -1;
    }
  }

  m_running = false;
}

} // namespace exec
