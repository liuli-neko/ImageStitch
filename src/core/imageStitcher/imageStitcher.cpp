#include "imageStitcher.hpp"

#include <glog/logging.h>
#include <math.h>
#include <omp.h>

#include <fstream>
#include <functional>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>
#include <sstream>

namespace ImageStitch {

const double PI = acos(-1);

namespace {
class CallAbleBase {
 public:
  virtual ~CallAbleBase() {}
  template <typename ResultT, typename... Args>
  void operator()(ResultT &out, Args &&...args);
  template <typename ResultT, typename... Args>
  ResultT call(Args &&...args);
};
template <typename ResultT, typename... Args>
class CallAble : public CallAbleBase {
 public:
  typedef std::function<ResultT(Args...)> InvokeFn;
  InvokeFn invoke_ptr;

 public:
  CallAble(InvokeFn invoke_ptr) : invoke_ptr(invoke_ptr) {}
  ~CallAble() {}
  ResultT call(Args &&...args) {
    return invoke_ptr(std::forward<Args>(args)...);
  }
};

template <typename ResultT, typename... Args>
void CallAbleBase::operator()(ResultT &out, Args &&...args) {
  out = dynamic_cast<CallAble<ResultT, Args...> *>(this)->call(
      std::forward<Args>(args)...);
}
template <typename ResultT, typename... Args>
ResultT CallAbleBase::call(Args &&...args) {
  return dynamic_cast<CallAble<ResultT, Args...> *>(this)->call(
      std::forward<Args>(args)...);
}

template <class T, typename ResultT, typename... Args>
static std::shared_ptr<CallAbleBase> CreateCallAble(ResultT (T::*func)(Args...),
                                                    T *self) {
  auto invoke_ptr = [self, func](Args... args) -> ResultT {
    self->func(std::forward<Args>(args)...);
  };
  return std::shared_ptr<CallAbleBase>(
      new CallAble<ResultT, Args...>(invoke_ptr));
}

template <typename ResultT, typename... Args>
static std::shared_ptr<CallAbleBase> CreateCallAble(
    std::function<ResultT(Args...)> func) {
  return std::shared_ptr<CallAbleBase>(new CallAble<ResultT, Args...>(func));
}
static std::map<std::string, std::shared_ptr<CallAbleBase>> ALL_CONFIGS;
static std::map<std::string, ConfigItem> CONFIG_TABLE;
static std::vector<std::string> CONFIG_ITEMS;

void CreateConfigItem(const std::string &item_name,
                      const ConfigItem::Type &config_type,
                      const std::string &description = "") {
  CONFIG_ITEMS.push_back(item_name);
  CONFIG_TABLE.insert(std::make_pair(
      item_name, ConfigItem(item_name, config_type, description)));
}
template <typename ResultT, typename... Args>
void RegisterOptionIntoConfig(const std::string &item_name,
                              const std::string &option_name,
                              ResultT (*func)(Args...)) {
  auto item = CONFIG_TABLE.find(item_name);
  if (item == CONFIG_TABLE.end()) {
    LOG(WARNING) << "Couldn't find config item : " << item_name
                 << " create new STRING config.";
    CreateConfigItem(item_name, ConfigItem::STRING);
    item = CONFIG_TABLE.find(item_name);
  }
  item->second.options.push_back(option_name);
  ALL_CONFIGS.insert(
      std::make_pair(item_name + "." + option_name,
                     CreateCallAble(std::function<ResultT(Args...)>(func))));
}
template <typename ValueT>
void _RegisterOptionIntoConfig(const std::string &item_name,
                               const ValueT &min_value, const ValueT &max_value,
                               std::true_type) {
  auto item = CONFIG_TABLE.find(item_name);
  if (item == CONFIG_TABLE.end()) {
    LOG(WARNING) << "Couldn't find config item : " << item_name
                 << " create new STRING config.";
    CreateConfigItem(item_name, ConfigItem::INT);
    item = CONFIG_TABLE.find(item_name);
  }
  item->second.range[0] = min_value;
  item->second.range[1] = max_value;
}
template <typename ValueT>
void _RegisterOptionIntoConfig(const std::string &item_name,
                               const ValueT &min_value, const ValueT &max_value,
                               std::false_type) {
  auto item = CONFIG_TABLE.find(item_name);
  if (item == CONFIG_TABLE.end()) {
    LOG(WARNING) << "Couldn't find config item : " << item_name
                 << " create new STRING config.";
    CreateConfigItem(item_name, ConfigItem::FLOAT);
    item = CONFIG_TABLE.find(item_name);
  }
  item->second.range[0] = min_value;
  item->second.range[1] = max_value;
}
template <typename ValueT>
void RegisterOptionIntoConfig(const std::string &item_name,
                              const ValueT &min_value,
                              const ValueT &max_value) {
  _RegisterOptionIntoConfig(item_name, min_value, max_value,
                            std::is_integral<ValueT>::type());
}

class EstimatorListener : public cv::detail::Estimator {
 public:
  EstimatorListener(cv::Ptr<cv::detail::Estimator> Estimator,
                    ImageStitcher *stitcher = nullptr)
      : _estimator(Estimator), _stitcher(stitcher) {}
  bool estimate(
      const std::vector<ImageFeatures> &features,
      const std::vector<MatchesInfo> &pairwise_matches,
      CV_OUT std::vector<cv::detail::CameraParams> &cameras) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Estimating Image ", -1);
    }
    bool result = (*_estimator)(features, pairwise_matches, cameras);
    LOG(INFO) << "Estimate finished" << std::endl;
    return result;
  }

 private:
  cv::Ptr<cv::detail::Estimator> _estimator;
  ImageStitcher *_stitcher;
};

class FeaturesMatcherListener : public cv::detail::FeaturesMatcher {
 public:
  FeaturesMatcherListener(cv::Ptr<cv::detail::FeaturesMatcher> features_matcher,
                          ImageStitcher *stitcher = nullptr)
      : _features_matcher(features_matcher),
        _stitcher(stitcher),
        cv::detail::FeaturesMatcher(features_matcher->isThreadSafe()) {}
  void match(const ImageFeatures &features1, const ImageFeatures &features2,
             MatchesInfo &matches_info) {
    (*_features_matcher)(features1, features2, matches_info);
    if (_stitcher != nullptr) {
      if (_stitcher->ImagesFeatures().size() < features1.img_idx + 1) {
        _stitcher->ImagesFeatures().resize(features1.img_idx + 1);
      }
      if (_stitcher->ImagesFeatures().size() < features2.img_idx + 1) {
        _stitcher->ImagesFeatures().resize(features2.img_idx + 1);
      }
      _stitcher->ImagesFeatures()[features1.img_idx] = features1;
      _stitcher->ImagesFeatures()[features2.img_idx] = features2;
      matches_info.src_img_idx = features1.img_idx;
      matches_info.dst_img_idx = features2.img_idx;
      _stitcher->FeaturesMatches()[std::make_pair(
          features1.img_idx, features2.img_idx)] = matches_info;
    }
    LOG(INFO) << "Features Matched";
    LOG(INFO) << "features1 : " << features1.img_idx;
    LOG(INFO) << "features2 : " << features2.img_idx;
    LOG(INFO) << "MatchesInfo : \n"
              << "src_img : " << matches_info.src_img_idx
              << " - dst_img : " << matches_info.dst_img_idx
              << "\nnum_inliers : " << matches_info.num_inliers
              << "\nconfidence : " << matches_info.confidence
              << "\nH : " << matches_info.H;
  }
  void collectGarbage() { _features_matcher->collectGarbage(); }

 private:
  cv::Ptr<cv::detail::FeaturesMatcher> _features_matcher;
  ImageStitcher *_stitcher;
};

class WarperListener : public cv::detail::RotationWarper {
 public:
  WarperListener(cv::Ptr<cv::detail::RotationWarper> Warper,
                 ImageStitcher *stitcher = nullptr)
      : _warper(Warper), _stitcher(stitcher) {}

  cv::Point2f warpPoint(const cv::Point2f &pt, cv::InputArray K,
                        cv::InputArray R) override {
    auto result = _warper->warpPoint(pt, K, R);
    return result;
  }
  cv::Point2f warpPointBackward(const cv::Point2f &pt, cv::InputArray K,
                                cv::InputArray R) {
    return _warper->warpPoint(pt, K, R);
  }
  cv::Rect buildMaps(cv::Size src_size, cv::InputArray K, cv::InputArray R,
                     cv::OutputArray xmap, cv::OutputArray ymap) override {
    return _warper->buildMaps(src_size, K, R, xmap, ymap);
  }
  cv::Point warp(cv::InputArray src, cv::InputArray K, cv::InputArray R,
                 int interp_mode, int border_mode,
                 CV_OUT cv::OutputArray dst) override {
    return _warper->warp(src, K, R, interp_mode, border_mode, dst);
  }
  void warpBackward(cv::InputArray src, cv::InputArray K, cv::InputArray R,
                    int interp_mode, int border_mode, cv::Size dst_size,
                    CV_OUT cv::OutputArray dst) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message.notify("Warping", -1);
    }
    _warper->warpBackward(src, K, R, interp_mode, border_mode, dst_size, dst);
    LOG(INFO) << "Warping backward finished";
  }
  cv::Rect warpRoi(cv::Size src_size, cv::InputArray K,
                   cv::InputArray R) override {
    return _warper->warpRoi(src_size, K, R);
  }
  float getScale() const { return _warper->getScale(); }
  void setScale(float scale) { _warper->setScale(scale); }

 private:
  cv::Ptr<cv::detail::RotationWarper> _warper;
  ImageStitcher *_stitcher;
};
class TWarperCreator : public cv::WarperCreator {
 public:
  TWarperCreator(cv::Ptr<cv::WarperCreator> creator,
                 ImageStitcher *stitcher = nullptr)
      : _creator(creator), _stitcher(stitcher) {}
  cv::Ptr<cv::detail::RotationWarper> create(float scale) const override {
    return new WarperListener(_creator->create(scale), _stitcher);
  }

 private:
  cv::Ptr<cv::WarperCreator> _creator;
  ImageStitcher *_stitcher;
};

class BundleAdjusterListener : public cv::detail::BundleAdjusterBase {
 public:
  BundleAdjusterListener(
      cv::Ptr<cv::detail::BundleAdjusterBase> bundle_adjuster,
      ImageStitcher *stitcher = nullptr)
      : _bundle_adjuster(bundle_adjuster),
        _stitcher(stitcher),
        cv::detail::BundleAdjusterBase(0, 0) {}

 private:
  bool estimate(const std::vector<ImageFeatures> &features,
                const std::vector<MatchesInfo> &pairwise_matches,
                std::vector<cv::detail::CameraParams> &cameras) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message.notify("BundleAdjusting", -1);
    }
    bool result = (*_bundle_adjuster)(features, pairwise_matches, cameras);
    if (_stitcher != nullptr) {
      _stitcher->FinalCameraParams() = cameras;
    }
    LOG(INFO) << "Bundle adjuster finished";
    return result;
  }
  void setUpInitialCameraParams(const std::vector<cv::detail::CameraParams> &)
      CV_OVERRIDE {}
  void obtainRefinedCameraParams(std::vector<cv::detail::CameraParams> &) const
      CV_OVERRIDE {}
  void calcError(cv::Mat &) CV_OVERRIDE {}
  void calcJacobian(cv::Mat &) CV_OVERRIDE {}

 private:
  cv::Ptr<cv::detail::BundleAdjusterBase> _bundle_adjuster;
  ImageStitcher *_stitcher;
};

class BlenderListener : public cv::detail::Blender {
 public:
  BlenderListener(cv::Ptr<cv::detail::Blender> Blender,
                  ImageStitcher *stitcher = nullptr)
      : _blender(Blender), _stitcher(stitcher) {}
  void prepare(const std::vector<cv::Point> &corners,
               const std::vector<cv::Size> &sizes) override {
    _blender->prepare(corners, sizes);
  }
  void prepare(cv::Rect dst_roi) override { _blender->prepare(dst_roi); }
  void feed(cv::InputArray img, cv::InputArray mask, cv::Point tl) override {
    _blender->feed(img, mask, tl);
  }
  void blend(cv::InputOutputArray dst, cv::InputOutputArray dst_mask) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message.notify("Blending", -1);
    }
    _blender->blend(dst, dst_mask);
    LOG(INFO) << "Blender finished";
  }

 private:
  cv::Ptr<cv::detail::Blender> _blender;
  ImageStitcher *_stitcher;
};

class SeamFinderListener : public cv::detail::SeamFinder {
 public:
  SeamFinderListener(cv::Ptr<cv::detail::SeamFinder> seam_finder,
                     ImageStitcher *stitcher = nullptr)
      : _seam_finder(seam_finder), _stitcher(stitcher) {}
  void find(const std::vector<cv::UMat> &src,
            const std::vector<cv::Point> &corners,
            std::vector<cv::UMat> &masks) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message.notify("Seam finding", -1);
    }
    _seam_finder->find(src, corners, masks);
    if (_stitcher != nullptr) {
      for (int i = 0; i < masks.size(); ++i) {
        cv::Mat img, dst;
        src[i].getMat(cv::ACCESS_READ).convertTo(img, CV_8UC3);
        img.copyTo(dst, masks[i]);
        _stitcher->SeamMasks().push_back(dst);
      }
    }
  }

 private:
  cv::Ptr<cv::detail::SeamFinder> _seam_finder;
  ImageStitcher *_stitcher;
};

class ExporterListener : public cv::detail::ExposureCompensator {
 public:
  ExporterListener(
      cv::Ptr<cv::detail::ExposureCompensator> exporter_compensator,
      ImageStitcher *stitcher = nullptr)
      : _exporter_compensator(exporter_compensator), _stitcher(stitcher) {}
  void feed(const std::vector<cv::Point> &corners,
            const std::vector<cv::UMat> &images,
            const std::vector<std::pair<cv::UMat, uchar>> &masks) override {
    LOG(INFO) << "Exporter compensator feed images" << std::endl;
    if (_stitcher != nullptr) {
      _stitcher->CompensatorImages().resize(images.size());
    }
    _exporter_compensator->feed(corners, images, masks);
  }
  void apply(int index, cv::Point corner, cv::InputOutputArray image,
             cv::InputArray mask) override {
    _exporter_compensator->apply(index, corner, image, mask);
    if (_stitcher != nullptr) {
      _stitcher->CompensatorImages().at(index).push_back(
          image.getMat().clone());
    }
    LOG(INFO) << "Exporter compensator finished : " << index;
  }
  void getMatGains(std::vector<cv::Mat> &mat) {
    _exporter_compensator->getMatGains(mat);
  }
  void setMatGains(std::vector<cv::Mat> &mat) {
    _exporter_compensator->setMatGains(mat);
  }

 private:
  cv::Ptr<cv::detail::ExposureCompensator> _exporter_compensator;
  ImageStitcher *_stitcher;
};

class FeatureDetectorListener : public cv::FeatureDetector {
 public:
  FeatureDetectorListener(cv::Ptr<cv::FeatureDetector> feature_detector,
                          ImageStitcher *stitcher = nullptr)
      : _feature_detector(feature_detector), _stitcher(stitcher) {}
  void detect(cv::InputArray image, std::vector<KeyPoint> &keypoints,
              cv::InputArray mask = cv::noArray()) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector detecting", -1);
    }
    _feature_detector->detect(image, keypoints, mask);
    LOG(INFO) << "Feature detector detected";
  }
  void detect(cv::InputArrayOfArrays images,
              std::vector<std::vector<KeyPoint>> &keypoints,
              cv::InputArrayOfArrays masks = cv::noArray()) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector detecting", -1);
    }
    _feature_detector->detect(images, keypoints, masks);
    LOG(INFO) << "Feature detector detected";
  }
  void compute(cv::InputArray image, std::vector<KeyPoint> &keypoints,
               cv::OutputArray descriptors) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector computing", -1);
    }
    _feature_detector->compute(image, keypoints, descriptors);
    LOG(INFO) << "Feature detector computed";
  }
  void compute(cv::InputArrayOfArrays images,
               std::vector<std::vector<KeyPoint>> &keypoints,
               cv::OutputArrayOfArrays descriptors) {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector computing", -1);
    }
    _feature_detector->compute(images, keypoints, descriptors);
    LOG(INFO) << "Feature detector computed";
  }
  void detectAndCompute(cv::InputArray image, cv::InputArray mask,
                        std::vector<KeyPoint> &keypoints,
                        cv::OutputArray descriptors,
                        bool useProvidedKeypoints = false) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector detecting and computing",
                                    -1);
    }
    _feature_detector->detectAndCompute(image, mask, keypoints, descriptors,
                                        useProvidedKeypoints);
    if (_stitcher != nullptr) {
      image.getMatVector(_stitcher->FinalStitchImages());
    }
  }

 private:
  cv::Ptr<cv::FeatureDetector> _feature_detector;
  ImageStitcher *_stitcher;
};

static void init() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;
  CreateConfigItem("Mode", ConfigItem::STRING,
                   "拼接模式，PANORAMA拼接模式为通用全景图模式。"
                   "SCANS假设图像是通过扫描仪或其他扫描设备拍摄的"
                   "平面的图。");

  RegisterOptionIntoConfig(
      "Mode", "PANORAMA",
      +[]() { return cv::Stitcher::create(cv::Stitcher::Mode::PANORAMA); });
  RegisterOptionIntoConfig(
      "Mode", "SCANS",
      +[]() { return cv::Stitcher::create(cv::Stitcher::Mode::SCANS); });

  CreateConfigItem("Estimator", ConfigItem::STRING,
                   "图像相机参数推断器，一般通过单应性矩阵推断参数。");
  RegisterOptionIntoConfig(
      "Estimator", "HomographyBasedEstimator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::Estimator> {
        return new EstimatorListener(new cv::detail::HomographyBasedEstimator(),
                                     stitcher);
      });
  RegisterOptionIntoConfig(
      "Estimator", "AffineBasedEstimator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::Estimator> {
        return new EstimatorListener(new cv::detail::AffineBasedEstimator(),
                                     stitcher);
      });

  CreateConfigItem("FeaturesFinder", ConfigItem::STRING,
                   "图像特征点提取器，用于提取图像中具备可描述特征的点，"
                   "主流特征提取算法有SIFT，SURF，ORB。");
  RegisterOptionIntoConfig(
      "FeaturesFinder", "ORB",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::FeatureDetector> {
        return new FeatureDetectorListener(
            cv::Ptr<cv::FeatureDetector>(cv::ORB::create()), stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesFinder", "SIFT",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::FeatureDetector> {
        return new FeatureDetectorListener(
            cv::Ptr<cv::FeatureDetector>(cv::SIFT::create()), stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesFinder", "SURF",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::FeatureDetector> {
        return new FeatureDetectorListener(
            cv::Ptr<cv::FeatureDetector>(cv::xfeatures2d::SURF::create()),
            stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesFinder", "BRISK",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::FeatureDetector> {
        return new FeatureDetectorListener(
            cv::Ptr<cv::FeatureDetector>(cv::BRISK::create()), stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesFinder", "KAZE",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::FeatureDetector> {
        return new FeatureDetectorListener(
            cv::Ptr<cv::FeatureDetector>(cv::KAZE::create()), stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesFinder", "AKAZE",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::FeatureDetector> {
        return new FeatureDetectorListener(
            cv::Ptr<cv::FeatureDetector>(cv::AKAZE::create()), stitcher);
      });

  CreateConfigItem("FeaturesMatcher", ConfigItem::STRING,
                   "特征匹配器，用于匹配两张图像中的特征点，主流匹配算法有。");
  RegisterOptionIntoConfig(
      "FeaturesMatcher", "BestOf2NearestMatcher",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::FeaturesMatcher> {
        return new FeaturesMatcherListener(
            new cv::detail::BestOf2NearestMatcher(true), stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesMatcher", "BestOf2NearestRangeMatcher",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::FeaturesMatcher> {
        return new FeaturesMatcherListener(
            new cv::detail::BestOf2NearestRangeMatcher(true), stitcher);
      });
  RegisterOptionIntoConfig(
      "FeaturesMatcher", "AffineBestOf2NearestMatcher",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::FeaturesMatcher> {
        return new FeaturesMatcherListener(
            new cv::detail::AffineBestOf2NearestMatcher(true), stitcher);
      });

  CreateConfigItem("Warper", ConfigItem::STRING,
                   "投影类型，用于将图像投影到平面或球面上，主流投影类型有平面"
                   "投影，柱面投影，球面投影等。");
  RegisterOptionIntoConfig(
      "Warper", "PlaneWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::PlaneWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "CylindricalWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::CylindricalWarper>(),
                                  stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "SphericalWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::SphericalWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "AffineWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::AffineWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "FisheyeWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::FisheyeWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "StereographicWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::StereographicWarper>(),
                                  stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "CompressedRectilinearWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(
            cv::makePtr<cv::CompressedRectilinearWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "CompressedRectilinearPortraitWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(
            cv::makePtr<cv::CompressedRectilinearPortraitWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "PaniniPortraitWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::PaniniPortraitWarper>(),
                                  stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "MercatorWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::MercatorWarper>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "TransverseMercatorWarper",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::TransverseMercatorWarper>(),
                                  stitcher);
      });
#ifdef HAVE_OPENCV_CUDAWARPING
  RegisterOptionIntoConfig(
      "Warper", "PlaneWarperGpu",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::PlaneWarperGpu>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "CylindricalWarperGpu",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::CylindricalWarperGpu>(),
                                  stitcher);
      });
  RegisterOptionIntoConfig(
      "Warper", "SphericalWarperGpu",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::WarperCreator> {
        return new TWarperCreator(cv::makePtr<cv::SphericalWarperGpu>(),
                                  stitcher);
      });
#endif

  CreateConfigItem("BundleAdjuster", ConfigItem::STRING,
                   "联合绑定优化器，在相机参数初步推断完成后，"
                   "被选中的所有全景图图像会全部给他做一次参数优化。");
  RegisterOptionIntoConfig(
      "BundleAdjuster", "NoBundleAdjuster",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::BundleAdjusterBase> {
        return new BundleAdjusterListener(
            cv::makePtr<cv::detail::NoBundleAdjuster>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "BundleAdjuster", "BundleAdjusterReproj",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::BundleAdjusterBase> {
        return new BundleAdjusterListener(
            cv::makePtr<cv::detail::BundleAdjusterReproj>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "BundleAdjuster", "BundleAdjusterRay",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::BundleAdjusterBase> {
        return new BundleAdjusterListener(
            cv::makePtr<cv::detail::BundleAdjusterRay>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "BundleAdjuster", "BundleAdjusterAffine",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::BundleAdjusterBase> {
        return new BundleAdjusterListener(
            cv::makePtr<cv::detail::BundleAdjusterAffine>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "BundleAdjuster", "BundleAdjusterAffinePartial",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::BundleAdjusterBase> {
        return new BundleAdjusterListener(
            cv::makePtr<cv::detail::BundleAdjusterAffinePartial>(), stitcher);
      });

  CreateConfigItem("Blender", ConfigItem::STRING,
                   "图像融合器，用于将拼接后的图像进行融合，"
                   "主流融合算法有多频段融合，羽化融合等。");
  RegisterOptionIntoConfig(
      "Blender", "NoBlender",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::Blender> {
        return new BlenderListener(
            cv::detail::Blender::createDefault(cv::detail::Blender::NO),
            stitcher);
      });
  RegisterOptionIntoConfig(
      "Blender", "MultiBandBlender",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::Blender> {
        return new BlenderListener(
            cv::detail::Blender::createDefault(cv::detail::Blender::MULTI_BAND),
            stitcher);
      });
  RegisterOptionIntoConfig(
      "Blender", "FeatherBlender",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::Blender> {
        return new BlenderListener(
            cv::detail::Blender::createDefault(cv::detail::Blender::FEATHER),
            stitcher);
      });

  CreateConfigItem(
      "SeamFinder", ConfigItem::STRING,
      "拼接缝合线查找器，用于查找拼接图像之间的缝合线，"
      "主流查找算法有暴力查找，动态规划查找，基于图像分割的查找等。");
  RegisterOptionIntoConfig(
      "SeamFinder", "NoSeamFinder",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::SeamFinder> {
        return new SeamFinderListener(cv::makePtr<cv::detail::NoSeamFinder>(),
                                      stitcher);
      });
  RegisterOptionIntoConfig(
      "SeamFinder", "DpSeamFinder",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::SeamFinder> {
        return new SeamFinderListener(cv::makePtr<cv::detail::DpSeamFinder>(),
                                      stitcher);
      });
  RegisterOptionIntoConfig(
      "SeamFinder", "VoronoiSeamFinder",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::SeamFinder> {
        return new SeamFinderListener(
            cv::makePtr<cv::detail::VoronoiSeamFinder>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "SeamFinder", "GraphCutSeamFinder",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::SeamFinder> {
        return new SeamFinderListener(
            cv::makePtr<cv::detail::GraphCutSeamFinder>(), stitcher);
      });

  CreateConfigItem(
      "ExposureCompensator", ConfigItem::STRING,
      "曝光补偿器，用于对拼接图像进行曝光补偿，主流曝光补偿算法有无补偿，增"
      "益补偿，通道补偿，块增益补偿，块通道补偿等。");
  RegisterOptionIntoConfig(
      "ExposureCompensator", "NoExposureCompensator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::ExposureCompensator> {
        return new ExporterListener(
            cv::makePtr<cv::detail::NoExposureCompensator>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "ExposureCompensator", "GainCompensator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::ExposureCompensator> {
        return new ExporterListener(cv::makePtr<cv::detail::GainCompensator>(),
                                    stitcher);
      });
  RegisterOptionIntoConfig(
      "ExposureCompensator", "ChannelsCompensator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::ExposureCompensator> {
        return new ExporterListener(
            cv::makePtr<cv::detail::ChannelsCompensator>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "ExposureCompensator", "BlocksGainCompensator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::ExposureCompensator> {
        return new ExporterListener(
            cv::makePtr<cv::detail::BlocksGainCompensator>(), stitcher);
      });
  RegisterOptionIntoConfig(
      "ExposureCompensator", "BlocksChannelsCompensator",
      +[](ImageStitcher *stitcher) -> cv::Ptr<cv::detail::ExposureCompensator> {
        return new ExporterListener(
            cv::makePtr<cv::detail::BlocksChannelsCompensator>(), stitcher);
      });

  CreateConfigItem("InterpolationFlags", ConfigItem::STRING,
                   "插值方式，用于图像缩放和旋转时的像素插值，"
                   "主流插值方式有最近邻插值，双线性插值，"
                   "双三次插值，面积插值，Lanczos插值等。");
  RegisterOptionIntoConfig(
      "InterpolationFlags", "INTER_NEAREST",
      +[]() -> cv::InterpolationFlags { return cv::INTER_NEAREST; });
  RegisterOptionIntoConfig(
      "InterpolationFlags", "INTER_LINEAR",
      +[]() -> cv::InterpolationFlags { return cv::INTER_LINEAR; });
  RegisterOptionIntoConfig(
      "InterpolationFlags", "INTER_CUBIC",
      +[]() -> cv::InterpolationFlags { return cv::INTER_CUBIC; });
  RegisterOptionIntoConfig(
      "InterpolationFlags", "INTER_AREA",
      +[]() -> cv::InterpolationFlags { return cv::INTER_AREA; });
  RegisterOptionIntoConfig(
      "InterpolationFlags", "INTER_LANCZOS4",
      +[]() -> cv::InterpolationFlags { return cv::INTER_LANCZOS4; });

  CreateConfigItem(
      "CompositingResol", ConfigItem::FLOAT,
      "图像将经过该系数缩放后进行融合操作，较小的缩放系数可以让处理更快但丢"
      "失的细节越多越容易出现匹配错误或不匹配的情况。较大的系数在图像过大时"
      "需要更多的资源，计算公式为min(1.0 qrt(compositing_resol * 1e6 / "
      "image_area)),如果小于零则不做缩放处理。");
  RegisterOptionIntoConfig("CompositingResol", -1.0, 1e10);

  CreateConfigItem(
      "RegistrationResol", ConfigItem::FLOAT,
      "图像将经过该系数缩放后进行预预处理操作，较小的缩放系数可以让处理更快但"
      "丢失的细节越多越容易出现匹配错误或不匹配的情况。较大的系数在图像过大时"
      "需要更多的资源，计算公式为min(1.0 qrt(register_resol * 1e6 / "
      "image_area))。");
  RegisterOptionIntoConfig("RegistrationResol", 0.0, 1e10);

  CreateConfigItem(
      "SeamEstimationResol", ConfigItem::FLOAT,
      "图像将经过该系数缩放后进行拼接缝求取，较小的缩放系数可以让处理更快但"
      "丢失的细节越多越容易出现匹配错误或不匹配的情况。较大的系数在图像过大时"
      "需要更多的资源，计算公式为min(1.0 qrt(seam_estimation_resol * 1e6 / "
      "image_area))。建议值为0.1");
  RegisterOptionIntoConfig("SeamEstimationResol", 0.0, 1e10);

  CreateConfigItem("PanoConfidenceThresh", ConfigItem::FLOAT,
                   "图像参数估计和标定优化的阈值，建议值为1");
  RegisterOptionIntoConfig("PanoConfidenceThresh", 0.0, 1e10);

  CreateConfigItem("DivideImage", ConfigItem::STRING,
                   "是否对图像进行带重叠的切割以提高拼接成功的概率");
  RegisterOptionIntoConfig(
      "DivideImage", "NO", +[]() -> int { return 0; });
  RegisterOptionIntoConfig(
      "DivideImage", "ROW", +[]() -> int { return 1; });
  RegisterOptionIntoConfig(
      "DivideImage", "COL", +[]() -> int { return 2; });
}

}  // namespace

auto ImageStitcher::ParamTable() -> std::vector<ConfigItem> {
  init();
  std::vector<ConfigItem> table;
  for (const auto &title : CONFIG_ITEMS) {
    table.push_back(CONFIG_TABLE[title]);
  }
  return table;
}
auto ImageStitcher::SetParams(const Parameters &params) -> void {
  if (!params.Empty()) {
    _params = params;
  }
  // LOG(INFO) << _params.ToString();
  _cv_stitcher.release();
  // 拼接模式
  auto stitcher_mode = "Mode." + _params.GetParam("Mode", std::string());
  _mode = Mode::ALL;
  if (ALL_CONFIGS.find(stitcher_mode) != ALL_CONFIGS.end()) {
    LOG(INFO) << stitcher_mode;
    if (stitcher_mode == "Mode.INCREMENTAL") {
      _mode = Mode::INCREMENTAL;
    }
    _cv_stitcher = ALL_CONFIGS.at(stitcher_mode)->call<cv::Ptr<cv::Stitcher>>();
    _current_stitcher_mode = stitcher_mode;
  } else {
    _cv_stitcher = cv::Stitcher::create();
    _current_stitcher_mode = "Mode.PANORAMA";
  }

  // 相机参数推断模型
  auto estimator_name =
      "Estimator." + _params.GetParam("Estimator", std::string());
  if (ALL_CONFIGS.find(estimator_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << estimator_name;
    _cv_stitcher->setEstimator(
        ALL_CONFIGS.at(estimator_name)
            ->call<cv::Ptr<cv::detail::Estimator>, ImageStitcher *>(this));
  }

  // 图像特征点提取器
  auto features_finder_name =
      "FeaturesFinder." + _params.GetParam("FeaturesFinder", std::string());
  if (ALL_CONFIGS.find(features_finder_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << features_finder_name;
    _cv_stitcher->setFeaturesFinder(
        ALL_CONFIGS.at(features_finder_name)
            ->call<cv::Ptr<cv::FeatureDetector>, ImageStitcher *>(this));
  }

  // 特征匹配器
  auto features_matcher_name =
      "FeaturesMatcher." + _params.GetParam("FeaturesMatcher", std::string());
  if (ALL_CONFIGS.find(features_matcher_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << features_matcher_name;
    _cv_stitcher->setFeaturesMatcher(
        ALL_CONFIGS.at(features_matcher_name)
            ->call<cv::Ptr<cv::detail::FeaturesMatcher>, ImageStitcher *>(
                this));
  }

  // 设置投影类型
  auto warper_name = "Warper." + _params.GetParam("Warper", std::string());
  if (ALL_CONFIGS.find(warper_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << warper_name;
    _cv_stitcher->setWarper(
        ALL_CONFIGS.at(warper_name)
            ->call<cv::Ptr<cv::WarperCreator>, ImageStitcher *>(this));
  }

  // 接缝查找器
  auto seam_finder_name =
      "SeamFinder." + _params.GetParam("SeamFinder", std::string());
  if (ALL_CONFIGS.find(seam_finder_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << seam_finder_name;
    _cv_stitcher->setSeamFinder(
        ALL_CONFIGS.at(seam_finder_name)
            ->call<cv::Ptr<cv::detail::SeamFinder>, ImageStitcher *>(this));
  }

  // 光照补偿
  auto exposure_compensator_name =
      "ExposureCompensator." +
      _params.GetParam("ExposureCompensator", std::string());
  if (ALL_CONFIGS.find(exposure_compensator_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << exposure_compensator_name;
    _cv_stitcher->setExposureCompensator(
        ALL_CONFIGS.at(exposure_compensator_name)
            ->call<cv::Ptr<cv::detail::ExposureCompensator>, ImageStitcher *>(
                this));
  }

  // 联合绑定优化
  auto bundle_adjuster_name =
      "BundleAdjuster." + _params.GetParam("BundleAdjuster", std::string());
  if (ALL_CONFIGS.find(bundle_adjuster_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << bundle_adjuster_name;
    _cv_stitcher->setBundleAdjuster(
        ALL_CONFIGS.at(bundle_adjuster_name)
            ->call<cv::Ptr<cv::detail::BundleAdjusterBase>, ImageStitcher *>(
                this));
  }

  // 图像融合模式
  auto blender_name = "Blender." + _params.GetParam("Blender", std::string());
  if (ALL_CONFIGS.find(blender_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << blender_name;
    _cv_stitcher->setBlender(
        ALL_CONFIGS.at(blender_name)
            ->call<cv::Ptr<cv::detail::Blender>, ImageStitcher *>(this));
  }

  // 图像插值方式
  auto interpolation_flags_name =
      "InterpolationFlags." +
      _params.GetParam("InterpolationFlags", std::string());
  if (ALL_CONFIGS.find(interpolation_flags_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << interpolation_flags_name;
    _cv_stitcher->setInterpolationFlags(
        ALL_CONFIGS.at(interpolation_flags_name)
            ->call<cv::InterpolationFlags>());
  }

  auto compositing_resol = _params.GetParam("CompositingResol", (float)-1.0);
  LOG(INFO) << "CompositingResol : " << compositing_resol;
  _cv_stitcher->setCompositingResol(compositing_resol);

  auto registration_resol = _params.GetParam("RegistrationResol", (float)0.6);
  LOG(INFO) << "RegistrationResol : " << registration_resol;
  _cv_stitcher->setRegistrationResol(registration_resol);

  auto seam_est_resol = _params.GetParam("SeamEstimationResol", (float)0.1);
  LOG(INFO) << "SeamEstimationResol : " << seam_est_resol;
  _cv_stitcher->setSeamEstimationResol(seam_est_resol);

  auto conf_thresh = _params.GetParam("PanoConfidenceThresh", (float)1.0);
  LOG(INFO) << "PanoConfidenceThresh : " << conf_thresh;
  _cv_stitcher->setPanoConfidenceThresh(conf_thresh);

  auto divide_image_name =
      "DivideImage." + _params.GetParam("DivideImage", std::string("NO"));
  if (ALL_CONFIGS.find(divide_image_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << "DivideImage : " << divide_image_name;
    _divide_images = ALL_CONFIGS.at(divide_image_name)->call<int>();
  }

  signal_run_message("配置成功.", 1000);
}
ImageStitcher::ImageStitcher() {
  init();
  if (std::filesystem::exists("./configuration.json")) {
    _params.Load("./configuration.json");
  } else {
    _params.FromString(
        "{"
        "\"CompositingResol\": {\"value\": -1.0},"
        "\"DivideImage\": {\"value\": \"NO\"},"
        "\"PanoConfidenceThresh\": {\"value\": 1.0},"
        "\"RegistrationResol\": {\"value\": 0.6},"
        "\"SeamEstimationResol\": {\"value\": 0.1},"
        "\"Blender\": {\"value\": \"MultiBandBlender\"},"
        "\"BundleAdjuster\": {\"value\": \"BundleAdjusterAffine\"},"
        "\"Estimator\": {\"value\": \"AffineBasedEstimator\"},"
        "\"ExposureCompensator\": {\"value\": \"NoExposureCompensator\"},"
        "\"FeaturesFinder\": {\"value\": \"SURF\"},"
        "\"FeaturesMatcher\": {\"value\": \"AffineBestOf2NearestMatcher\"},"
        "\"InterpolationFlags\": {\"value\": \"INTER_LINEAR\"},"
        "\"Mode\": {\"value\": \"SCANS\"},"
        "\"SeamFinder\": {\"value\": \"GraphCutSeamFinder\"},"
        "\"Warper\": {\"value\": \"AffineWarper\"}"
        "}");
  }
}

auto ImageStitcher::Clean() -> bool {
  _images.clear();
  _final_images.clear();
  _compensator_images.clear();
  _seam_masks.clear();
  _images_features.clear();
  _features_matches.clear();
  _final_camera_params.clear();
  _pairwise_matches.clear();
  _camera_params.clear();
  _regist_scales.clear();
  _comp.clear();
  _cv_stitcher.release();

  return true;
}

auto ImageStitcher::SetImages(std::vector<ImagePtr> images) -> bool {
  _images.clear();
  _images = images;
  return true;
}

auto ImageStitcher::SetImages(std::vector<std::string> image_files) -> bool {
  _images.resize(image_files.size());
#pragma omp parallel for
  for (int i = 0; i < image_files.size(); ++i) {
    std::ifstream file;
    file.open(image_files[i], std::ios::binary);
    std::vector<uchar> buffer((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    _images[i] = new Image(cv::imdecode(buffer, cv::IMREAD_COLOR));
  }
  return true;
}

double ImageStitcher::Distance(double lat1, double lon1, double lat2,
                               double lon2) {
  double R = 6371004;  // m
  double dLat = (lat2 - lat1) * PI / 180;
  double dLon = (lon2 - lon1) * PI / 180;
  lat1 = lat1 * PI / 180;
  lat2 = lat2 * PI / 180;

  double a = sin(dLat / 2) * sin(dLat / 2) +
             sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  double d = R * c;
  return d;
}

const Image ImageStitcher::WarperImageByCameraParams(const int index1,
                                                     const int index2) {
  if (index1 < 0 || index1 >= _final_images.size()) {
    return Image();
  }
  int i = std::lower_bound(_comp.begin(), _comp.end(), index1) - _comp.begin();
  if (i >= _comp.size() || _comp[i] != index1) {
    return Image();
  }
  if (index2 < 0 || index2 >= _final_images.size()) {
    return Image();
  }
  int j = std::lower_bound(_comp.begin(), _comp.end(), index2) - _comp.begin();
  if (j >= _comp.size() || _comp[j] != index2) {
    return Image();
  }
  if (_features_matches.find({index1, index2}) == _features_matches.end()) {
    return Image();
  }
  if (index1 == index2) {
    return Image();
  }

  auto w = _cv_stitcher->warper()->create(
      (_final_camera_params[i].focal + _final_camera_params[j].focal) / 2);

  Mat K;
  _final_camera_params[i].K().convertTo(K, CV_32F);
  cv::Rect roi1 =
      w->warpRoi(_final_images[index1].size(), K, _final_camera_params[i].R);
  Image img_warped1;
  w->warp(_final_images[index1], K, _final_camera_params[i].R,
          _cv_stitcher->interpolationFlags(), cv::BORDER_TRANSPARENT,
          img_warped1);

  _final_camera_params[j].K().convertTo(K, CV_32F);
  cv::Rect roi2 =
      w->warpRoi(_final_images[index2].size(), K, _final_camera_params[j].R);
  Image img_warped2;
  w->warp(_final_images[index2], K, _final_camera_params[j].R,
          _cv_stitcher->interpolationFlags(), cv::BORDER_TRANSPARENT,
          img_warped2);
  cv::Point2i c1 = roi1.tl();
  cv::Point2i c2 = roi2.tl();
  c2 = c2 - c1;
  c1 = cv::Point2i(0, 0);
  int width = (std::max)(c1.x + roi1.size().width, c2.x + roi2.size().width) -
              (std::min)(c1.x, c2.x);
  int height =
      (std::max)(c1.y + roi1.size().height, c2.y + roi2.size().height) -
      (std::min)(c1.y, c2.y);
  if (c2.x < 0) {
    c1.x -= c2.x;
    c2.x = 0;
  }
  if (c2.y < 0) {
    c1.y -= c2.y;
    c2.y = 0;
  }

  Image result(height, width, _final_images[index1].type());

  Image half1(result, {c1, roi1.size()});
  half1 += 0.5 * img_warped1;
  Image half2(result, {c2, roi2.size()});
  half2 += 0.5 * img_warped2;

  return result;
}

const Image ImageStitcher::WarperImageByHomography(const int index1,
                                                   const int index2) {
  if (index1 < 0 || index1 >= _final_images.size()) {
    return Image();
  }
  int i = std::lower_bound(_comp.begin(), _comp.end(), index1) - _comp.begin();
  if (i >= _comp.size() || _comp[i] != index1) {
    return Image();
  }
  if (index2 < 0 || index2 >= _final_images.size()) {
    return Image();
  }
  int j = std::lower_bound(_comp.begin(), _comp.end(), index2) - _comp.begin();
  if (j >= _comp.size() || _comp[j] != index2) {
    return Image();
  }
  if (_features_matches.find({index1, index2}) == _features_matches.end()) {
    return Image();
  }
  if (index1 == index2) {
    return Image();
  }

  auto matches = _features_matches[{index1, index2}];
  Image dst;
  cv::Mat points =
      (cv::Mat_<double>(3, 4) << 0, _final_images[index1].cols, 0,
       _final_images[index1].cols, 0, 0, _final_images[index1].rows,
       _final_images[index1].rows, 1, 1, 1, 1);

  points = matches.H * points;
  int x1 = (std::min)({points.at<double>(0, 0), points.at<double>(0, 1),
                       points.at<double>(0, 2), points.at<double>(0, 3)});
  int y1 = (std::min)({points.at<double>(1, 0), points.at<double>(1, 1),
                       points.at<double>(1, 2), points.at<double>(1, 3)});
  int x2 = (std::max)({points.at<double>(0, 0), points.at<double>(0, 1),
                       points.at<double>(0, 2), points.at<double>(0, 3)});
  int y2 = (std::max)({points.at<double>(1, 0), points.at<double>(1, 1),
                       points.at<double>(1, 2), points.at<double>(1, 3)});

  int width = (std::max)(x2, _final_images[index2].cols) - (std::min)(x1, 0);
  int height = (std::max)(y2, _final_images[index2].rows) - (std::min)(y1, 0);
  cv::warpPerspective(_final_images[index1], dst, matches.H,
                      cv::Size(width, height));
  Image result(height, width, _final_images[index1].type());

  result += 0.5 * dst;
  Image half2(result,
              cv::Rect((std::max)(-x1, 0), (std::max)(-y1, 0),
                       _final_images[index2].cols, _final_images[index2].rows));
  half2 += 0.5 * _final_images[index2];

  return result;
}

auto ImageStitcher::Stitch(std::vector<Image> &images)
    -> std::vector<ImagePtr> {
  std::vector<ImagePtr> results;
  Image result;
  signal_run_message("开始拼接", -1);
  auto status = _cv_stitcher->stitch(images, result);
  std::string str;
  if (status == cv::Stitcher::OK) {
    str = "已完成拼接:";
    _comp = _cv_stitcher->component();
    for (int i : _comp) {
      if (_divide_images == 1 || _divide_images == 2) {
        str +=
            std::to_string(i / 3 + 1) + "-" + std::to_string(i % 3 + 1) + ", ";
      } else {
        str += std::to_string(i + 1) + ", ";
      }
    }
    results.push_back(new Image(result));
    signal_run_message(str, -1);
  } else {
    signal_run_message("拼接失败,错误代码: " + std::to_string(status), -1);
  }
  return results;
}

auto ImageStitcher::IncrementalStitch(std::vector<Image> &images)
    -> std::vector<ImagePtr> {
  signal_run_message.notify("开始拼接", -1);
  std::vector<ImagePtr> results;
  std::vector<int> indices;
  indices.push_back(0);
  std::vector<ImageFeatures> features(images.size());
  std::map<std::pair<int, int>, MatchesInfo> matches;
  _camera_params.resize(images.size());
  _camera_params[0].resize(2);
  _comp.push_back(0);
  for (int i = 1; i < images.size(); ++i) {
    _comp.push_back(i);
    signal_run_message("estimate camera params " + std::to_string(i) + "/" +
                           std::to_string(images.size() - 1),
                       -1);
    auto status = _cv_stitcher->estimateTransform(
        std::vector<Image>{images[i - 1], images[i]});
    if (i == images.size() - 1) {
      features[i] = _images_features[1];
      features[i].img_idx = i;
    }
    features[i - 1] = _images_features[0];
    features[i - 1].img_idx = i - 1;

    matches[{i - 1, i}] = _features_matches[{0, 1}];
    matches[{i - 1, i}].src_img_idx = i - 1;
    matches[{i - 1, i}].dst_img_idx = i;
    matches[{i, i - 1}] = _features_matches[{1, 0}];
    matches[{i, i - 1}].src_img_idx = i;
    matches[{i, i - 1}].dst_img_idx = i - 1;
    LOG(INFO) << "Features " << features[i - 1].img_idx;

    if (status != cv::Stitcher::OK) {
      signal_run_message("参数估计失败，" + std::to_string(status), -1);
      indices.push_back(i);
      _camera_params[i].resize(2);
    } else {
      _camera_params[i] = _cv_stitcher->cameras();
    }
  }
  _images_features = features;
  _features_matches = matches;
  FinalCameraParams().resize(images.size());
  indices.push_back(images.size());
  signal_run_message("warpering ...", -1);
  for (int i = 1; i < indices.size(); ++i) {
    if (indices[i] - indices[i - 1] <= 1) {
      results.push_back(new Image(images[indices[i - 1]]));
    } else {
      std::vector<CameraParams> camera_params;
      camera_params.resize(indices[i] - indices[i - 1]);
      cv::Mat R_inv;
      for (size_t j = 0; j < camera_params.size(); ++j) {
        if (j == indices[i - 1]) {
          camera_params[j] = _camera_params[j + indices[i - 1]][1];
          camera_params[j].R = cv::Mat::eye(cv::Size(3, 3), CV_32F);
        } else {
          camera_params.at(j) = _camera_params.at(j + indices.at(i - 1)).at(1);
          cv::Mat R_j, R_0, R_1;
          camera_params[j - 1].R.convertTo(R_j, CV_32F);
          _camera_params[j + indices[i - 1]][0].R.convertTo(R_0, CV_32F);
          _camera_params[j + indices.at(i - 1)][1].R.convertTo(R_1, CV_32F);
          camera_params[j].R = R_j * R_0.inv() * R_1;
          std::cout << camera_params[j].t << std::endl;
        }
        FinalCameraParams()[j + indices[i - 1]] = camera_params[j];
      }
      std::vector<Image> current_images;
      for (int j = indices[i - 1]; j < indices[i]; ++j) {
        current_images.push_back(images[j]);
      }
      auto status = _cv_stitcher->setTransform(current_images, camera_params);
      if (status == cv::Stitcher::OK) {
        Image pano;
        status = _cv_stitcher->composePanorama(pano);
        if (status == cv::Stitcher::OK) {
          results.push_back(new Image(pano));
        }
      }
    }
  }
  signal_run_message("拼接完成", -1);
  return results;
}

auto ImageStitcher::MergeStitch(std::vector<Image> &images, const int s,
                                const int e) -> std::vector<ImagePtr> {
  if (e - s <= 1) {
    if (e == s) {
      return std::vector<ImagePtr>();
    }
    return std::vector<ImagePtr>{new Image(images[s])};
  }
  if (e - s <= 4) {
    Image pano;
    auto status = _cv_stitcher->stitch(
        std::vector<Image>(images.begin() + s, images.begin() + e), pano);
    if (status == cv::Stitcher::OK) {
      return std::vector<ImagePtr>{new Image(pano)};
    }
  }
  int mid = (s + e) / 2;

  std::vector<ImagePtr> pano1 = MergeStitch(images, s, mid);
  std::vector<ImagePtr> pano2 = MergeStitch(images, mid, e);

  Image pano;
  auto status = _cv_stitcher->stitch(
      std::vector<Image>{*pano1.back(), *pano2.front()}, pano);
  std::vector<ImagePtr> results;
  if (pano1.size() > 1) {
    std::for_each(pano1.begin(), pano1.end() - 1,
                  [&results](ImagePtr img) { results.push_back(img); });
  }
  if (status == cv::Stitcher::OK) {
    results.push_back(new Image(pano));
  } else {
    results.push_back(pano1.back());
    results.push_back(pano2.front());
  }
  if (pano2.size() > 1) {
    std::for_each(pano2.begin() + 1, pano2.end(),
                  [&results](ImagePtr img) { results.push_back(img); });
  }
  return results;
}

auto ImageStitcher::Stitch() -> std::vector<ImagePtr> {
  if (_cv_stitcher.empty()) {
    SetParams(Parameters());
  }
  if (_images.size() <= 0) {
    signal_run_message("请提供图像", -1);
    signal_result(std::vector<ImagePtr>());
    return std::vector<ImagePtr>();
  }
  signal_run_message("预备拼接图像", -1);
  std::vector<Image> images_;
  for (int i = 0; i < _images.size(); ++i) {
    if (_divide_images == 0 || _mode != ALL) {
      images_.push_back(*_images[i]);
    } else if (_divide_images == 1) {
      cv::Rect rect(0, 0, _images[i]->cols, _images[i]->rows / 2);
      images_.push_back((*_images[i])(rect).clone());
      rect.y = _images[i]->rows / 3;
      images_.push_back((*_images[i])(rect).clone());
      rect.y = _images[i]->rows / 2;
      images_.push_back((*_images[i])(rect).clone());
    } else if (_divide_images == 2) {
      cv::Rect rect(0, 0, _images[i]->cols / 2, _images[i]->rows);
      images_.push_back((*_images[i])(rect).clone());
      rect.x = _images[i]->cols / 3;
      images_.push_back((*_images[i])(rect).clone());
      rect.x = _images[i]->cols / 2;
      images_.push_back((*_images[i])(rect).clone());
    } else {
      images_.push_back(*_images[i]);
    }
  }
  FinalStitchImages().clear();
  // FinalStitchImages() = images_;
  _regist_scales.resize(images_.size());
  FinalStitchImages().resize(images_.size());
  for (int i = 0; i < images_.size(); ++i) {
    _regist_scales[i] =
        (std::min)(1.0, std::sqrt(_cv_stitcher->registrationResol() * 1e6 /
                                  images_[i].size().area()));
    resize(images_[i], FinalStitchImages()[i], cv::Size(), _regist_scales[i],
           _regist_scales[i], cv::INTER_LINEAR_EXACT);
  }
  std::vector<ImagePtr> results;
  if (_mode == Mode::ALL) {
    results = Stitch(images_);
    FinalCameraParams() = _cv_stitcher->cameras();
  } else if (_mode == Mode::INCREMENTAL) {
    results = IncrementalStitch(images_);
  } else if (_mode == Mode::MERGE) {
    results = MergeStitch(images_, 0, images_.size());
    signal_run_message("拼接完成。", -1);
  } else {
    signal_run_message("未知拼接模式", 10000);
  }
  signal_result(results);
  signal_run_progress(1);
  return results;
}

auto ImageStitcher::ImageSize() -> int { return _images.size(); }

auto ImageStitcher::DetectFeatures(const Image &image) -> ImageFeatures {
  if (!_cv_stitcher.empty()) {
    ImageFeatures image_features;
    KeyPoints keypoints;
    Descriptors descriptors;
    image_features.img_idx = -1;
    image_features.img_size = image.size();
    _cv_stitcher->featuresFinder()->detectAndCompute(image, cv::Mat(),
                                                     keypoints, descriptors);
    image_features.keypoints = std::move(keypoints);
    image_features.descriptors = std::move(descriptors);
    return image_features;
  } else {
    LOG(ERROR) << "未初始化stitcher!";
  }
  return ImageFeatures();
}

auto ImageStitcher::MatchesFeatures(const ImageFeatures &features1,
                                    const ImageFeatures &features2)
    -> std::vector<MatchesInfo> {
  if (!_cv_stitcher.empty()) {
    std::vector<MatchesInfo> matches_info;
    (*(_cv_stitcher->featuresMatcher()))({features1, features2}, matches_info);
    _cv_stitcher->featuresMatcher()->collectGarbage();
    return matches_info;
  } else {
    LOG(ERROR) << "未初始化stitcher!";
  }
  return std::vector<MatchesInfo>();
}

auto ImageStitcher::EstimateCameraParams(
    const std::vector<ImageFeatures> &features,
    const std::vector<MatchesInfo> &pairwise_matches)
    -> std::vector<CameraParams> {
  std::vector<CameraParams> camera_params;
  if (!_cv_stitcher.empty()) {
    if (!(*_cv_stitcher->estimator())(features, pairwise_matches,
                                      camera_params)) {
      LOG(ERROR) << "Homography estimate failed";
    }
  } else {
    LOG(ERROR) << "未初始化stitcher!";
  }
  return camera_params;
}

Image ImageStitcher::DrawKeypoint(const Image &image,
                                  const ImageFeatures &image_features) {
  Image image_;
  cv::drawKeypoints(image, image_features.keypoints, image_);
  return image_;
}

Image ImageStitcher::DrawMatches(const Image &image1, const ImageFeatures &f1,
                                 const Image &image2, const ImageFeatures &f2,
                                 MatchesInfo &matches) {
  Image image;
  std::vector<cv::DMatch> good_matches;
  const auto &all_matches = matches.getMatches();
  const auto &inliers = matches.getInliers();
  for (int i = 0; i < all_matches.size(); ++i) {
    if (inliers[i]) {
      good_matches.push_back(all_matches[i]);
    }
  }
  cv::drawMatches(image1, f1.keypoints, image2, f2.keypoints, good_matches,
                  image);
  return image;
}

auto ImageStitcher::RemoveImage(int index) -> bool {
  if (index < 0 || index >= _images.size()) {
    return false;
  }
  _images.erase(_images.begin() + index);
  return true;
}
auto ImageStitcher::RemoveAllImages() -> bool {
  _images.clear();
  return true;
}
auto ImageStitcher::GetImage(int index) -> ImagePtr {
  if (index < 0 || index >= _images.size()) {
    return nullptr;
  }
  return _images[index];
}
auto ImageStitcher::GetImages() -> std::vector<ImagePtr> { return _images; }
};  // namespace ImageStitch