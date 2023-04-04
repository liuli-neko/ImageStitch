#include "parameters.hpp"

#include <fstream>

namespace ImageStitch {
auto Parameters::HasParam(const std::string &param_name) -> bool {
  return params.find(param_name) != params.end();
}

auto Parameters::Save(const std::string &file_name) -> void {
  std::ofstream out_file;
  out_file.open(file_name, std::ios_base::out);
  if (out_file.is_open()) {
    out_file << params.dump();
  }
}

auto Parameters::Load(const std::string &file_name) -> void {
  std::ifstream input_json_file(file_name);
  params << input_json_file;
}

auto Parameters::ToString() -> std::string { return params.dump(); }
auto Parameters::FromString(const char *str) -> void {
  params = nlohmann::json::parse(str);
}

void to_json(nlohmann::json &j, const Parameters &params) { j = params.params; }

void from_json(const nlohmann::json &j, Parameters &params) {
  params.params = j;
}

}  // namespace ImageStitch