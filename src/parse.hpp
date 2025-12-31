#pragma once

#include <string>
#include <vector>

namespace parse {
struct Rule {
  std::string name;
  std::vector<std::string> deps;
  std::vector<std::string> commands;
};

struct Result {
  std::vector<std::string> phony;
  std::vector<::parse::Rule> rules;
};

class MakefileParser {
public:
  ::parse::Result parse(const std::vector<std::string> &lines) const;
};

} // namespace parse
