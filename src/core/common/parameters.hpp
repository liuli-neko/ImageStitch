#pragma once

#include <glog/logging.h>

#include <any>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>

namespace ImageStitch {

class Parameters {
 private:
  nlohmann::json params;

 public:
  template <typename T>
  auto SetParam(const std::string &param_name, const T &value,
                const bool is_cover = true) -> void {
    nlohmann::json param;
    if (params.find(param_name) == params.end()) {
      param["value"] = value;
      params[param_name] = param;
    } else if (is_cover) {
      LOG(WARNING) << "covered " << param_name << ":\n" << params[param_name];
      params[param_name]["value"] = value;
    }
  }
  template <typename T>
  auto GetParam(const std::string &param_name, const T &default_value) const
      -> T;
  auto HasParam(const std::string &param_name) -> bool;
  auto Save(const std::string &file_name) -> void;
  auto Load(const std::string &file_name) -> void;
  auto ToString() -> std::string;
  auto FromString(const char *str) -> void;
  auto Empty() const -> bool;

  friend void to_json(nlohmann::json &j, const Parameters &params);
  friend void from_json(const nlohmann::json &j, Parameters &params);
};
template <typename T>
auto Parameters::GetParam(const std::string &param_name,
                          const T &default_value) const -> T {
  auto pm = params.find(param_name);
  if (pm == params.end()) {
    LOG(WARNING) << param_name << " not exists!";
    return default_value;
  }
  return (*pm).value<T>("value", default_value);
}
void to_json(nlohmann::json &j, const Parameters &params);

void from_json(const nlohmann::json &j, Parameters &params);
}  // namespace ImageStitch
