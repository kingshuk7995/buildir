#pragma once
#include <string>
#include <vector>

class FileReader {
public:
  explicit FileReader(std::string path);
  std::vector<std::string> read_lines() const;

private:
  std::string m_path;
};
