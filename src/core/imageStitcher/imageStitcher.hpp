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

struct ImageExif {
  uint focal_length_35mm = 0;
  int flash_used = 0;
  int iso_speed = 0;
  int image_width = 0;
  int image_height = 0;
  int image_orientation = 0;
  int exposure_time = 0;
  float f_stop = 0;
  float exposure_bias = 0;
  float focal_length = 0;
  float gps_latitude = 0;
  float gps_longitude = 0;
  float gps_altitude = 0;
  std::string camera_model = "";
  std::string original_date_time = "";
};

class ImageStitcher {
public:
  enum Mode { ALL = 0, INCREMENTAL = 1 };

public:
  ImageStitcher();
  auto SetParams(const Parameters &params = Parameters()) -> void;
  auto SetImages(std::vector<ImagePtr> images) -> bool;
  auto SetImages(std::vector<std::string> image_files) -> bool;
  auto RemoveImage(int index) -> bool;
  auto RemoveAllImages() -> bool;
  auto GetImage(int index) -> ImagePtr;
  auto GetImages() -> std::vector<ImagePtr>;
  auto Stitch() -> std::vector<ImagePtr>;
  auto Features(const Image &image) -> ImageFeatures;
  auto Matcher(const ImageFeatures &features1, const ImageFeatures &features2)
      -> std::vector<MatchesInfo>;
  auto EstimateCameraParams(const std::vector<ImageFeatures> &features,
                            const std::vector<MatchesInfo> &pairwise_matches)
      -> std::vector<CameraParams>;
  auto ImageSize() -> int;
  auto Clean() -> bool;
  inline const Parameters &GetParams() const { return _params; }
  inline Parameters &GetParams() { return _params; }

  static auto ParamTable() -> std::vector<ConfigItem>;

public:
  Signal<void(std::string, int)> signal_run_message;
  Signal<void(float)> signal_run_progress;
  Signal<void(std::vector<ImagePtr>)> signal_result;

public:
  Image DrawKeypoint(const Image &image, const ImageFeatures &image_features);
  Image DrawMatches(const Image &image1, const ImageFeatures &f1,
                    const Image &image2, const ImageFeatures &f2,
                    MatchesInfo &matches);

private:
  /**
   * @brief 直接使用所有的图像进行拼接操作。同时会自动舍弃无法完成拼接的图像。
   *
   * @param image
   * @return ImagePtr
   */
  auto Stitch(std::vector<Image> &images) -> std::vector<ImagePtr>;
  /**
   * @brief
   * 假设图像顺序已经有序，将会按顺序逐张拼接。同时该方法在无法完全拼接的情况下会给出多个拼接结果。
   *
   * @param image
   * @return ImagePtr
   */
  auto IncrementalStitch(std::vector<Image> &images) -> std::vector<ImagePtr>;

private:
  Parameters _params;
  cv::Ptr<cv::Stitcher> _cv_stitcher;
  std::vector<ImagePtr> _images;
  std::vector<ImageFeatures> _images_features;
  std::vector<ImageExif> _image_exifs;
  std::vector<std::vector<MatchesInfo>> _pairwise_matches;
	std::vector<std::vector<CameraParams>> _camera_params;
  std::string _current_stitcher_mode;
  int _divide_images;
  Mode _mode;
};
} // namespace ImageStitch
