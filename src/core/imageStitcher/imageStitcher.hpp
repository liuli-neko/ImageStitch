#pragma once

#include <memory>
#include <opencv2/opencv.hpp>
#include <vector>

#include "../../signal/trackable.hpp"
#include "../common/cvTypeDef.hpp"
#include "../common/parameters.hpp"

namespace ImageStitch {

struct ConfigItem {
  std::string title;
  std::vector<std::string> options;
  std::string description;
};

class ImageStitcher {
 private:
  Parameters params;
  cv::Ptr<cv::Stitcher> cv_stitcher;
  std::vector<ImagePtr> images;

 public:
  ImageStitcher();
  auto SetParams(const Parameters& params = Parameters()) -> void;
  auto AddImage(ImagePtr image) -> bool;
  auto RemoveImage(int index) -> bool;
  auto RemoveAllImages() -> bool;
  auto GetImage(int index) -> ImagePtr;
  auto GetImages() -> std::vector<ImagePtr>;
  auto Stitch() -> ImagePtr;
  inline const Parameters& GetParams() const { return params; }
  inline Parameters GetParams() { return params; }

  static auto ParamTable() -> std::vector<ConfigItem>;

 public:
  Signal<void(std::string, int)> signal_run_message;
  Signal<void(float)> signal_run_progress;
  Signal<void(ImagePtr)> signal_result;
};
}  // namespace ImageStitch
