#include <exec.hpp>
#include <file_reader.hpp>
#include <filesystem>
#include <optional>
#include <parse.hpp>
#include <thread>

int main(int argc, char *argv[]) {
  const char *filename = "Makefile";
  ArgsResult res = ArgsResult::parse_and_filter(argc, argv);
  std::string task = res.forwarded_args.size()
                         ? std::string(res.forwarded_args[0])
                         : exec::default_cmd;
  uint32_t njobs;
  if (res.thread_count.has_value() == false) {
    njobs = exec::default_procs;
  } else if (*res.thread_count == 0) {
    njobs = std::thread::hardware_concurrency();
  } else {
    njobs = static_cast<uint32_t>(*res.thread_count);
  }

  if (!std::filesystem::exists(std::filesystem::path(filename))) {
    fatal("Makefile not found");
  }

  auto [g, ser_needed] = [&filename]() -> std::pair<exec::Graph, bool> {
    if (is_newer(exec::Graph::serialize_file, filename)) {
      return {exec::Graph::deserialize(), false};
    } else {
      FileReader reader(filename);
      const auto lines = reader.read_lines();

      parse::MakefileParser parser;
      auto parsed_data = parser.parse(lines);
      for (const auto &pd : parsed_data.phony) {
        std::cout << "phony: " << pd << '\n';
      }

      return {exec::Graph::build(parsed_data), true};
    }
  }();

  exec::Scheduler s(njobs);
  s.start_pool();

  std::optional<std::jthread> bg_serialize;

  if (ser_needed) {
    auto work = [](const exec::Graph &graph) { graph.serialize(); };
    bg_serialize.emplace(work, std::ref(g));
  }
  s.run(g, task);

  return 0;
}
