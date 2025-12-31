#include <file_reader.hpp>
#include <fstream>
#include <utils.hpp>

FileReader::FileReader(std::string path) : m_path(std::move(path)) {}

std::vector<std::string> FileReader::read_lines() const {
  std::ifstream file(m_path);
  if (!file) {
    fatal("failed to open file");
  }
  std::vector<std::string> lines;
  std::string line;

  while (std::getline(file, line)) {
    trim(line);
    if (line.empty()) {
      continue;
    }

    auto comment = line.find('#');
    if (comment == 0) {
      continue;
    }
    if (comment != std::string::npos) {
      line.erase(comment);
    }

    if (!line.empty()) {
      lines.push_back(line);
    }
  }

  return lines;
}
