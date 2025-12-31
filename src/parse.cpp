#include <cstdlib>
#include <iostream>
#include <parse.hpp>
#include <ranges>
#include <string_view>
#include <utils.hpp>

namespace parse {

::parse::Result
MakefileParser::parse(const std::vector<std::string> &lines) const {
  Result result;

  Rule current;
  bool in_rule = false;

  for (const std::string &line : lines) {

    // .PHONY
    if (line.starts_with(".PHONY:")) {
      std::string_view rest(line.c_str() + 7);
      for (auto part : rest | std::views::split(' ')) {
        if (!part.empty())
          result.phony.emplace_back(part.begin(), part.end());
      }
      continue;
    }

    // command
    if (!line.empty() && line[0] == '\t') {
      if (!in_rule) {
        fatal("command without target");
      }

      if (line.size() > 1)
        current.commands.emplace_back(line.begin() + 1, line.end());

      continue;
    }

    // new rule
    if (in_rule) {
      result.rules.push_back(std::move(current));
      current = {};
    }

    auto colon = line.find(':');
    if (colon == std::string::npos) {
      std::cout << "line: " << line << '\n';
      fatal("invalid rule (missing ':')");
    }
    current.name.assign(line.begin(), line.begin() + static_cast<long>(colon));

    std::string_view deps(line.begin() + static_cast<long>(colon + 1),
                          line.end());
    for (auto dep : deps | std::views::split(' ')) {
      if (!dep.empty())
        current.deps.emplace_back(dep.begin(), dep.end());
    }

    in_rule = true;
  }

  if (in_rule)
    result.rules.push_back(std::move(current));

  return result;
}

} // namespace parse
