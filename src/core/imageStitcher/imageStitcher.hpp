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
  ConfigItem(
      const std::string &title = "", const Type &type = STRING,
      const std::string &desc = "",
      const std::vector<std::string> &options = std::vector<std::string>())
      : title(title), type(type), description(desc), options(options) {}
  std::string title;
  Type type;
  std::string description;
  std::vector<std::string> options;
  double range[2];
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
  enum Mode { ALL = 0, INCREMENTAL = 1, MERGE = 2 };

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
  auto DetectFeatures(const Image &image) -> ImageFeatures;
  auto MatchesFeatures(const ImageFeatures &features1,
                       const ImageFeatures &features2)
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
  double Distance(double lat1, double lon1, double lat2, double lon2);

  inline std::vector<Image> &FinalStitchImages() { return _final_images; }
  inline const std::vector<Image> &FinalStitchImages() const {
    return _final_images;
  }
  const Image WarperImageByCameraParams(const int index1, const int index2);
  const Image WarperImageByHomography(const int index1, const int index2);
  inline std::vector<ImageFeatures> &ImagesFeatures() {
    return _images_features;
  }
  inline const std::vector<ImageFeatures> &ImagesFeatures() const {
    return _images_features;
  }
  inline std::map<std::pair<int, int>, MatchesInfo> &FeaturesMatches() {
    return _features_matches;
  }
  inline const std::map<std::pair<int, int>, MatchesInfo> &FeaturesMatches()
      const {
    return _features_matches;
  }
  inline std::vector<CameraParams> &FinalCameraParams() {
    return _final_camera_params;
  }
  inline const std::vector<CameraParams> &FinalCameraParams() const {
    return _final_camera_params;
  }
  inline std::vector<std::vector<Image>> &CompensatorImages() {
    return _compensator_images;
  }
  inline const std::vector<std::vector<Image>> &CompensatorImages() const {
    return _compensator_images;
  }
  inline const std::vector<int> &component() { return _comp; }
  inline std::vector<Image> &SeamMasks() { return _seam_masks; }
  inline const std::vector<Image> &SeamMasks() const { return _seam_masks; }

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
  /**
   * @brief
   * 假设图像顺序已经有序，将会按归并的方式拼接。同时该方法在无法完全拼接的情况下会给出多个拼接结果。
   *
   * @param images
   * @return std::vector<ImagePtr>
   */
  auto MergeStitch(std::vector<Image> &images, const int s, const int e)
      -> std::vector<ImagePtr>;

 private:
  Parameters _params;
  cv::Ptr<cv::Stitcher> _cv_stitcher;
  std::vector<ImagePtr> _images;
  std::vector<Image> _final_images;
  std::vector<std::vector<Image>> _compensator_images;
  std::vector<Image> _seam_masks;
  std::vector<ImageFeatures> _images_features;
  std::map<std::pair<int, int>, MatchesInfo> _features_matches;
  std::vector<CameraParams> _final_camera_params;
  std::vector<ImageExif> _image_exifs;
  std::vector<std::vector<MatchesInfo>> _pairwise_matches;
  std::vector<std::vector<CameraParams>> _camera_params;
  std::vector<double> _regist_scales;
  std::vector<int> _comp;
  std::string _current_stitcher_mode;
  int _divide_images;
  Mode _mode;
  const bool kUseGps = false;
};
}  // namespace ImageStitch
