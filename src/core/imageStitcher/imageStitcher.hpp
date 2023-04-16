#pragma once

#include <memory>
#include <opencv2/opencv.hpp>
#include <vector>

#include "../../signal/trackable.hpp"
#include "../common/cvTypeDef.hpp"
#include "../common/parameters.hpp"

namespace ImageStitch {

struct ConfigItem {
  enum Type { STRING = 0, FLOAT = 1, INT = 2 };
  std::string title;
  Type type;
  std::vector<std::string> options;
  double range[2];
  std::string description;
};

class ImageStitcher {
 public:
  enum Mode { ALL = 0, INCREMENTAL = 1 };

 public:
  ImageStitcher();
  auto SetParams(const Parameters& params = Parameters()) -> void;
  auto SetImages(std::vector<ImagePtr> images) -> bool;
  auto SetImages(std::vector<std::string> image_files) -> bool;
  auto RemoveImage(int index) -> bool;
  auto RemoveAllImages() -> bool;
  auto GetImage(int index) -> ImagePtr;
  auto GetImages() -> std::vector<ImagePtr>;
  auto Stitch() -> std::vector<ImagePtr>;
  auto Features(const int index) -> ImageFeatures;
  auto Matcher(const int index1, const int index2) -> MatchesInfo;
  auto ImageSize() -> int;
  auto Clean() -> bool;
  inline const Parameters& GetParams() const { return _params; }
  inline Parameters& GetParams() { return _params; }

  static auto ParamTable() -> std::vector<ConfigItem>;

 public:
  Signal<void(std::string, int)> signal_run_message;
  Signal<void(float)> signal_run_progress;
  Signal<void(std::vector<ImagePtr>)> signal_result;

 public:
  Image DrawKeypoint(const int index);
  Image DrawMatches(const int index1, const int index2);

 private:
  /**
   * @brief 直接使用所有的图像进行拼接操作。同时会自动舍弃无法完成拼接的图像。
   *
   * @param image
   * @return ImagePtr
   */
  auto Stitch(std::vector<Image>& images) -> std::vector<ImagePtr>;
  /**
   * @brief
   * 假设图像顺序已经有序，将会按顺序逐张拼接。同时该方法在无法完全拼接的情况下会给出多个拼接结果。
   *
   * @param image
   * @return ImagePtr
   */
  auto IncrementalStitch(std::vector<Image>& images) -> std::vector<ImagePtr>;

 private:
  Parameters _params;
  cv::Ptr<cv::Stitcher> _cv_stitcher;
  std::vector<ImagePtr> _images;
  std::vector<ImageFeatures> _images_features;
  std::map<std::pair<int, int>, MatchesInfo> _matches;
  std::string _current_stitcher_mode;
  int _divide_images;
  Mode _mode;
};
}  // namespace ImageStitch
