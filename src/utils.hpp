#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

[[noreturn]] inline void fatal(const char *msg) {
  std::cerr << msg << '\n';
  std::exit(EXIT_FAILURE);
}

inline void trim(std::string &s) {
  auto not_space = [](unsigned char c) { return c != ' '; };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

template <typename T> class Ref {
public:
  explicit Ref(T &t) noexcept : ptr(&t) {}
  explicit Ref(T *p) noexcept : ptr(p) {}

  T &operator*() const noexcept { return *ptr; }
  T *operator->() const noexcept { return ptr; }

  T &get() const noexcept { return *ptr; }

private:
  T *ptr;
};

inline bool is_newer(const std::string &file, const std::string &wrt) {
  namespace fs = std::filesystem;

  std::error_code ec1, ec2;

  auto ftime = fs::last_write_time(file, ec1);
  auto wtime = fs::last_write_time(wrt, ec2);

  if (ec2) {
    fatal("dependency output missing (internal error)");
  }

  if (ec1) {
    return false;
  }

  return ftime > wtime;
}

struct ArgsResult {
  std::optional<int> thread_count;
  std::vector<std::string_view> forwarded_args;

  static inline ArgsResult parse_and_filter(int argc, char *argv[]) {
    ArgsResult result;
    result.forwarded_args.reserve(static_cast<size_t>(argc - 1));

    for (int i = 1; i < argc; ++i) {
      std::string_view arg(argv[i]);

      if (arg.starts_with("-j")) {
        if (arg.size() > 2) {
          // Case: -j8
          int val;
          auto [ptr, ec] =
              std::from_chars(arg.data() + 2, arg.data() + arg.size(), val);
          if (ec == std::errc{})
            result.thread_count = val;
        } else if (i + 1 < argc) {
          // Case: -j 8
          std::string_view next_arg(argv[i + 1]);
          int val;
          auto [ptr, ec] = std::from_chars(
              next_arg.data(), next_arg.data() + next_arg.size(), val);

          if (ec == std::errc{}) {
            result.thread_count = val;
            i++;
          } else {
            result.thread_count = 0;
          }
        } else {
          result.thread_count = 0;
        }
      } else {
        result.forwarded_args.push_back(arg);
      }
    }
    return result;
  }
};
