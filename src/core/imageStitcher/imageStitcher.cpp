#include "imageStitcher.hpp"

#include <exif.h>
#include <glog/logging.h>
#include <math.h>

#include <fstream>
#include <functional>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>
#include <sstream>

namespace ImageStitch {

namespace {
class CallAbleBase {
 public:
  virtual ~CallAbleBase() {}
};
template <typename ResultT, typename... Args>
class CallAble : public CallAbleBase {
 public:
  typedef std::function<ResultT(Args...)> InvokeFn;
  InvokeFn invoke_ptr;

 public:
  CallAble(InvokeFn invoke_ptr) : invoke_ptr(invoke_ptr) {}
  ~CallAble() {}
  ResultT call(Args... args) { return invoke_ptr(std::forward<Args>(args)...); }
};
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
std::string GetDots() {
  static std::string dots[] = {".", "..", "...", "....", "....."};
  static int i = 0;
  return dots[i++ % 5];
}
static std::map<std::string, std::shared_ptr<CallAbleBase>> ALL_CONFIGS;
static std::vector<ConfigItem> CONFIG_TABLE;

class EstimatorListener : public cv::detail::Estimator {
 public:
  EstimatorListener(cv::Ptr<cv::detail::Estimator> estimator,
                    ImageStitcher *stitcher = nullptr)
      : _estimator(estimator), _stitcher(stitcher) {}
  bool estimate(
      const std::vector<ImageFeatures> &features,
      const std::vector<MatchesInfo> &pairwise_matches,
      CV_OUT std::vector<cv::detail::CameraParams> &cameras) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Estimating Image " + GetDots(), -1);
    }
    bool result = (*_estimator)(features, pairwise_matches, cameras);
    LOG(INFO) << "Estimate finished" << std::endl;
    // for (int i = 0; i < cameras.size(); ++i) {
    //   LOG(INFO) << "camera id: " << features[i].img_idx;
    //   LOG(INFO) << "E : \n"
    //             << cameras[i].focal << ", " << 0 << ", " << cameras[i].ppx
    //             << "\n"
    //             << "0"
    //             << ", " << cameras[i].focal << ", " << cameras[i].ppy << "\n"
    //             << 0 << ", " << 0 << ", " << 1;
    //   LOG(INFO) << "R : \n" << cameras[i].R;
    //   LOG(INFO) << "T : \n" << cameras[i].t;
    // }
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
    // if (_stitcher != nullptr) {
    //   _stitcher->signal_run_message.notify("Features matching" + GetDots(),
    //   -1);
    // }
    (*_features_matcher)(features1, features2, matches_info);
    // LOG(INFO) << "Features Matched";
    // LOG(INFO) << "MatchesInfo : \n"
    //           << "src_img : " << matches_info.src_img_idx
    //           << " - dst_img : " << matches_info.dst_img_idx
    //           << "\nnum_inliers : " << matches_info.num_inliers
    //           << "\nconfidence : " << matches_info.confidence
    //           << "\nH : " << matches_info.H;
  }
  void collectGarbage() { _features_matcher->collectGarbage(); }

 private:
  cv::Ptr<cv::detail::FeaturesMatcher> _features_matcher;
  ImageStitcher *_stitcher;
};

class WarperListener : public cv::detail::RotationWarper {
 public:
  WarperListener(cv::Ptr<cv::detail::RotationWarper> warper,
                 ImageStitcher *stitcher = nullptr)
      : _warper(warper), _stitcher(stitcher) {}

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
      _stitcher->signal_run_message.notify("Warping" + GetDots(), -1);
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
      _stitcher->signal_run_message.notify("BundleAdjusting" + GetDots(), -1);
    }
    bool result = (*_bundle_adjuster)(features, pairwise_matches, cameras);
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
  BlenderListener(cv::Ptr<cv::detail::Blender> blender,
                  ImageStitcher *stitcher = nullptr)
      : _blender(blender), _stitcher(stitcher) {}
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
      _stitcher->signal_run_message.notify("Blending" + GetDots(), -1);
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
      _stitcher->signal_run_message.notify("Seam finding" + GetDots(), -1);
    }
    _seam_finder->find(src, corners, masks);
    LOG(INFO) << "Seam finder finished" << std::endl;
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
    _exporter_compensator->feed(corners, images, masks);
  }
  void apply(int index, cv::Point corner, cv::InputOutputArray image,
             cv::InputArray mask) override {
    // if (_stitcher != nullptr) {
    //   _stitcher->signal_run_message.notify("Exporter Compensating" +
    //   GetDots(),
    //                                        -1);
    // }
    _exporter_compensator->apply(index, corner, image, mask);
    LOG(INFO) << "Exporter Compensator finished";
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
      _stitcher->signal_run_message("Feature detector detecting" + GetDots(),
                                    -1);
    }
    _feature_detector->detect(image, keypoints, mask);
    LOG(INFO) << "Feature detector detected";
  }
  void detect(cv::InputArrayOfArrays images,
              std::vector<std::vector<KeyPoint>> &keypoints,
              cv::InputArrayOfArrays masks = cv::noArray()) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector detecting" + GetDots(),
                                    -1);
    }
    _feature_detector->detect(images, keypoints, masks);
    LOG(INFO) << "Feature detector detected";
  }
  void compute(cv::InputArray image, std::vector<KeyPoint> &keypoints,
               cv::OutputArray descriptors) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector computing" + GetDots(),
                                    -1);
    }
    _feature_detector->compute(image, keypoints, descriptors);
    LOG(INFO) << "Feature detector computed";
    // LOG(INFO) << "detector keypoints: " << keypoints.size();
    // LOG(INFO) << "detector descriptors: " << descriptors.size();
  }
  void compute(cv::InputArrayOfArrays images,
               std::vector<std::vector<KeyPoint>> &keypoints,
               cv::OutputArrayOfArrays descriptors) {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message("Feature detector computing" + GetDots(),
                                    -1);
    }
    _feature_detector->compute(images, keypoints, descriptors);
    LOG(INFO) << "Feature detector computed";
    // LOG(INFO) << "detector keypoints: " << keypoints.size();
    // for_each(keypoints.begin(), keypoints.end(),
    //          [](const std::vector<KeyPoint> &k) {
    //            LOG(INFO) << "keypoint " << k.size();
    //          });
    // LOG(INFO) << "detector descriptors: " << descriptors.size();
  }
  void detectAndCompute(cv::InputArray image, cv::InputArray mask,
                        std::vector<KeyPoint> &keypoints,
                        cv::OutputArray descriptors,
                        bool useProvidedKeypoints = false) override {
    if (_stitcher != nullptr) {
      _stitcher->signal_run_message(
          "Feature detector detecting and computing" + GetDots(), -1);
    }
    _feature_detector->detectAndCompute(image, mask, keypoints, descriptors,
                                        useProvidedKeypoints);
    // LOG(INFO) << "Feature detector detected and computed";
    // LOG(INFO) << "detector keypoints: " << keypoints.size();
    // LOG(INFO) << "detector descriptors: " << descriptors.size();
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
  {
    ConfigItem item;
    item.title = "mode";
    item.type = ConfigItem::STRING;
    item.description =
        "拼接模式，PANORAMA拼接模式为通用全景图模式。"
        "SCANS假设图像是通过扫描仪或其他扫描设备拍摄的"
        "平面的图。";
    item.options.push_back("PANORAMA");
    ALL_CONFIGS.insert(std::make_pair(
        "mode.PANORAMA",
        CreateCallAble(std::function<cv::Ptr<cv::Stitcher>()>([]() {
          return cv::Stitcher::create(cv::Stitcher::Mode::PANORAMA);
        }))));
    item.options.push_back("SCANS");
    ALL_CONFIGS.insert(std::make_pair(
        "mode.SCANS",
        CreateCallAble(std::function<cv::Ptr<cv::Stitcher>()>([]() {
          return cv::Stitcher::create(cv::Stitcher::Mode::SCANS);
        }))));
    item.options.push_back("INCREMENTAL");
    ALL_CONFIGS.insert(std::make_pair(
        "mode.INCREMENTAL",
        CreateCallAble(std::function<cv::Ptr<cv::Stitcher>()>([]() {
          return cv::Stitcher::create(cv::Stitcher::Mode::SCANS);
        }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "estimator";
    item.type = ConfigItem::STRING;
    item.description = "图像相机参数推断器，一般通过单应性矩阵推断参数。";
    item.options.push_back("HomographyBasedEstimator");
    ALL_CONFIGS.insert(std::make_pair(
        "estimator.HomographyBasedEstimator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::Estimator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new EstimatorListener(
                  new cv::detail::HomographyBasedEstimator(), stitcher);
            }))));
    item.options.push_back("AffineBasedEstimator");
    ALL_CONFIGS.insert(std::make_pair(
        "estimator.AffineBasedEstimator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::Estimator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new EstimatorListener(
                  new cv::detail::AffineBasedEstimator(), stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "featuresFinder";
    item.type = ConfigItem::STRING;
    item.description =
        "图像特征点提取器，用于提取图像中具备可描述特征的点，"
        "主流特征提取算法有SIFT，SURF，ORB。";
    item.options.push_back("ORB");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.ORB",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::ORB::create()), stitcher);
            }))));
    item.options.push_back("SIFT");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.SIFT",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::SIFT::create()), stitcher);
            }))));
    item.options.push_back("SURF");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.SURF",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::xfeatures2d::SURF::create()),
                  stitcher);
            }))));
    item.options.push_back("BRISK");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.BRISK",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::BRISK::create()), stitcher);
            }))));
    item.options.push_back("MSER");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.MSER",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::MSER::create()), stitcher);
            }))));
    item.options.push_back("FastFeatureDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.FastFeatureDetector",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(
                      cv::FastFeatureDetector::create()),
                  stitcher);
            }))));
    item.options.push_back("AgastFeatureDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.AgastFeatureDetector",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(
                      cv::AgastFeatureDetector::create()),
                  stitcher);
            }))));
    item.options.push_back("GFTTDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.GFTTDetector",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::GFTTDetector::create()),
                  stitcher);
            }))));
    item.options.push_back("SimpleBlobDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.SimpleBlobDetector",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(
                      cv::SimpleBlobDetector::create()),
                  stitcher);
            }))));
    item.options.push_back("KAZE");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.KAZE",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::KAZE::create()), stitcher);
            }))));
    item.options.push_back("AKAZE");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.AKAZE",
        CreateCallAble(
            std::function<cv::Ptr<cv::FeatureDetector>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeatureDetectorListener(
                  cv::Ptr<cv::FeatureDetector>(cv::AKAZE::create()), stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "featuresMatcher";
    item.type = ConfigItem::STRING;
    item.description =
        "特征匹配器，用于匹配两张图像中的特征点，主流匹配算法有。";
    item.options.push_back("BestOf2NearestMatcher");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresMatcher.BestOf2NearestMatcher",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::FeaturesMatcher>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeaturesMatcherListener(
                  new cv::detail::BestOf2NearestMatcher(true), stitcher);
            }))));
    item.options.push_back("BestOf2NearestRangeMatcher");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresMatcher.BestOf2NearestRangeMatcher",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::FeaturesMatcher>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeaturesMatcherListener(
                  new cv::detail::BestOf2NearestRangeMatcher(true), stitcher);
            }))));
    item.options.push_back("AffineBestOf2NearestMatcher");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresMatcher.AffineBestOf2NearestMatcher",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::FeaturesMatcher>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new FeaturesMatcherListener(
                  new cv::detail::AffineBestOf2NearestMatcher(false, true),
                  stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "warper";
    item.type = ConfigItem::STRING;
    item.description =
        "投影类型，用于将图像投影到平面或球面上，主流投影类型有平面"
        "投影，柱面投影，球面投影等。";
    item.options.push_back("PlaneWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PlaneWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::PlaneWarper>(),
                                            stitcher);
                }))));
    item.options.push_back("CylindricalWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CylindricalWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::CylindricalWarper>(), stitcher);
                }))));
    item.options.push_back("SphericalWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.SphericalWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::SphericalWarper>(),
                                            stitcher);
                }))));
    item.options.push_back("AffineWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.AffineWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::AffineWarper>(),
                                            stitcher);
                }))));
    item.options.push_back("FisheyeWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.FisheyeWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::FisheyeWarper>(),
                                            stitcher);
                }))));
    item.options.push_back("StereographicWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.StereographicWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::StereographicWarper>(), stitcher);
                }))));
    item.options.push_back("CompressedRectilinearWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CompressedRectilinearWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::CompressedRectilinearWarper>(), stitcher);
                }))));
    item.options.push_back("CompressedRectilinearPortraitWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CompressedRectilinearPortraitWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::CompressedRectilinearPortraitWarper>(),
                      stitcher);
                }))));
    item.options.push_back("PaniniWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PaniniWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::PaniniWarper>(),
                                            stitcher);
                }))));
    item.options.push_back("PaniniPortraitWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PaniniPortraitWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::PaniniPortraitWarper>(), stitcher);
                }))));
    item.options.push_back("MercatorWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.MercatorWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::MercatorWarper>(),
                                            stitcher);
                }))));
    item.options.push_back("TransverseMercatorWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.TransverseMercatorWarper",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::TransverseMercatorWarper>(), stitcher);
                }))));
#ifdef HAVE_OPENCV_CUDAWARPING
    item.options.push_back("PlaneWarperGpu");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PlaneWarperGpu",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(cv::makePtr<cv::PlaneWarperGpu>(),
                                            stitcher);
                }))));
    item.options.push_back("CylindricalWarperGpu");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CylindricalWarperGpu",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::CylindricalWarperGpu>(), stitcher);
                }))));
    item.options.push_back("SphericalWarperGpu");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.SphericalWarperGpu",
        CreateCallAble(
            std::function<cv::Ptr<cv::WarperCreator>(ImageStitcher * stitcher)>(
                [](ImageStitcher *stitcher) {
                  return new TWarperCreator(
                      cv::makePtr<cv::SphericalWarperGpu>(), stitcher);
                }))));
#endif
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "bundleAdjuster";
    item.type = ConfigItem::STRING;
    item.description =
        "联合绑定优化器，在相机参数初步推断完成后，"
        "被选中的所有全景图图像会全部给他做一次参数优化。 ";
    item.options.push_back("NoBundleAdjuster");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.NoBundleAdjuster",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BundleAdjusterListener(
                  cv::makePtr<cv::detail::NoBundleAdjuster>(), stitcher);
            }))));
    item.options.push_back("BundleAdjusterReproj");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterReproj",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BundleAdjusterListener(
                  cv::makePtr<cv::detail::BundleAdjusterReproj>(), stitcher);
            }))));
    item.options.push_back("BundleAdjusterRay");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterRay",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BundleAdjusterListener(
                  cv::makePtr<cv::detail::BundleAdjusterRay>(), stitcher);
            }))));
    item.options.push_back("BundleAdjusterAffine");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterAffine",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BundleAdjusterListener(
                  cv::makePtr<cv::detail::BundleAdjusterAffine>(), stitcher);
            }))));
    item.options.push_back("BundleAdjusterAffinePartial");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterAffinePartial",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BundleAdjusterListener(
                  cv::makePtr<cv::detail::BundleAdjusterAffinePartial>(),
                  stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "blender";
    item.type = ConfigItem::STRING;
    item.description =
        "图像融合器，用于将拼接后的图像进行融合，"
        "主流融合算法有多频段融合，羽化融合等。";
    item.options.push_back("NoBlender");
    ALL_CONFIGS.insert(std::make_pair(
        "blender.NoBlender",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::Blender>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BlenderListener(
                  cv::detail::Blender::createDefault(cv::detail::Blender::NO),
                  stitcher);
            }))));
    item.options.push_back("MultiBandBlender");
    ALL_CONFIGS.insert(std::make_pair(
        "blender.MultiBandBlender",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::Blender>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BlenderListener(cv::detail::Blender::createDefault(
                                             cv::detail::Blender::MULTI_BAND),
                                         stitcher);
            }))));
    item.options.push_back("FeatherBlender");
    ALL_CONFIGS.insert(std::make_pair(
        "blender.FeatherBlender",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::Blender>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new BlenderListener(cv::detail::Blender::createDefault(
                                             cv::detail::Blender::FEATHER),
                                         stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "seamFinder";
    item.type = ConfigItem::STRING;
    item.description =
        "拼接缝合线查找器，用于查找拼接图像之间的缝合线，"
        "主流查找算法有暴力查找，动态规划查找，基于图像分割的查找等。 ";
    item.options.push_back("NoSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.NoSeamFinder",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::SeamFinder>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new SeamFinderListener(
                  cv::makePtr<cv::detail::NoSeamFinder>(), stitcher);
            }))));
    item.options.push_back("DpSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.DpSeamFinder",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::SeamFinder>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new SeamFinderListener(
                  cv::makePtr<cv::detail::DpSeamFinder>(), stitcher);
            }))));
    item.options.push_back("VoronoiSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.VoronoiSeamFinder",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::SeamFinder>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new SeamFinderListener(
                  cv::makePtr<cv::detail::VoronoiSeamFinder>(), stitcher);
            }))));
    item.options.push_back("GraphCutSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.GraphCutSeamFinder",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::SeamFinder>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new SeamFinderListener(
                  cv::makePtr<cv::detail::GraphCutSeamFinder>(
                      cv::detail::GraphCutSeamFinderBase::COST_COLOR),
                  stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "exposureCompensator";
    item.type = ConfigItem::STRING;
    item.description =
        "曝光补偿器，用于对拼接图像进行曝光补偿，主流曝光补偿算法有无补偿，增"
        "益补偿，通道补偿，块增益补偿，块通道补偿等。";
    item.options.push_back("NoExposureCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.NoExposureCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new ExporterListener(
                  cv::makePtr<cv::detail::NoExposureCompensator>(), stitcher);
            }))));
    item.options.push_back("GainCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.GainCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new ExporterListener(
                  cv::makePtr<cv::detail::GainCompensator>(), stitcher);
            }))));
    item.options.push_back("ChannelsCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.ChannelsCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new ExporterListener(
                  cv::makePtr<cv::detail::ChannelsCompensator>(), stitcher);
            }))));
    item.options.push_back("BlocksGainCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.BlocksGainCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new ExporterListener(
                  cv::makePtr<cv::detail::BlocksGainCompensator>(), stitcher);
            }))));
    item.options.push_back("BlocksChannelsCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.BlocksChannelsCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>(
                ImageStitcher * stitcher)>([](ImageStitcher *stitcher) {
              return new ExporterListener(
                  cv::makePtr<cv::detail::BlocksChannelsCompensator>(),
                  stitcher);
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "interpolationFlags";
    item.type = ConfigItem::STRING;
    item.description =
        "插值方式，用于图像缩放和旋转时的像素插值，"
        "主流插值方式有最近邻插值，双线性插值，"
        "双三次插值，面积插值，Lanczos插值等。";
    item.options.push_back("INTER_NEAREST");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_NEAREST",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_NEAREST; }))));
    item.options.push_back("INTER_LINEAR");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_LINEAR",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_LINEAR; }))));
    item.options.push_back("INTER_CUBIC");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_CUBIC",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_CUBIC; }))));
    item.options.push_back("INTER_AREA");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_AREA",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_AREA; }))));
    item.options.push_back("INTER_LANCZOS4");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_LANCZOS4",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_LANCZOS4; }))));
    item.options.push_back("INTER_LINEAR_EXACT");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_LINEAR_EXACT",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_LINEAR_EXACT; }))));
    item.options.push_back("INTER_NEAREST_EXACT");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_NEAREST_EXACT",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_NEAREST_EXACT; }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "CompositingResol";
    item.type = ConfigItem::FLOAT;
    item.description =
        "图像将经过该系数缩放后进行融合操作，较小的缩放系数可以让处理更快但丢"
        "失的细节越多越容易出现匹配错误或不匹配的情况。较大的系数在图像过大时"
        "需要更多的资源，计算公式为min(1.0 qrt(compositing_resol * 1e6 / "
        "image_area)),如果小于零则不做缩放处理。";
    item.range[0] = -1;
    item.range[1] = 1e308;
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "RegistrationResol";
    item.type = ConfigItem::FLOAT;
    item.description =
        "图像将经过该系数缩放后进行预预处理操作，较小的缩放系数可以让处理更快但"
        "丢失的细节越多越容易出现匹配错误或不匹配的情况。较大的系数在图像过大时"
        "需要更多的资源，计算公式为min(1.0 qrt(register_resol * 1e6 / "
        "image_area))。";
    item.range[0] = 0;
    item.range[1] = 1e308;
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "SeamEstimationResol";
    item.type = ConfigItem::FLOAT;
    item.description =
        "图像将经过该系数缩放后进行拼接缝求取，较小的缩放系数可以让处理更快但"
        "丢失的细节越多越容易出现匹配错误或不匹配的情况。较大的系数在图像过大时"
        "需要更多的资源，计算公式为min(1.0 qrt(seam_estimation_resol * 1e6 / "
        "image_area))。建议值为0.1";
    item.range[0] = 0;
    item.range[1] = 1e308;
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "PanoConfidenceThresh";
    item.type = ConfigItem::FLOAT;
    item.description = "图像参数估计和标定优化的阈值，建议值为1";
    item.range[0] = 0;
    item.range[1] = 1e308;
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "DivideImage";
    item.type = ConfigItem::STRING;
    item.description = "是否对图像进行带重叠的切割以提高拼接成功的概率";
    item.options.push_back("NO");
    ALL_CONFIGS.insert(std::make_pair(
        "DivideImage.NO",
        CreateCallAble(std::function<int()>([]() { return 0; }))));
    item.options.push_back("ROW");
    ALL_CONFIGS.insert(std::make_pair(
        "DivideImage.ROW",
        CreateCallAble(std::function<int()>([]() { return 1; }))));
    item.options.push_back("COL");
    ALL_CONFIGS.insert(std::make_pair(
        "DivideImage.COL",
        CreateCallAble(std::function<int()>([]() { return 2; }))));
    CONFIG_TABLE.push_back(item);
  }
}

}  // namespace

auto ImageStitcher::ParamTable() -> std::vector<ConfigItem> {
  init();
  return CONFIG_TABLE;
}
auto ImageStitcher::SetParams(const Parameters &params) -> void {
  if (!params.Empty()) {
    _params = params;
  }
  // LOG(INFO) << _params.ToString();
  _cv_stitcher.release();
  // 拼接模式
  auto stitcher_mode = "mode." + _params.GetParam("mode", std::string());
  _mode = Mode::ALL;
  if (ALL_CONFIGS.find(stitcher_mode) != ALL_CONFIGS.end()) {
    LOG(INFO) << stitcher_mode;
    if (stitcher_mode == "mode.INCREMENTAL") {
      _mode = Mode::INCREMENTAL;
    }
    _cv_stitcher = dynamic_cast<CallAble<cv::Ptr<cv::Stitcher>> *>(
                       ALL_CONFIGS.at(stitcher_mode).get())
                       ->call();
    _current_stitcher_mode = stitcher_mode;
  } else {
    _cv_stitcher = cv::Stitcher::create();
    _current_stitcher_mode = "mode.PANORAMA";
  }

  // 相机参数推断模型
  auto estimator_name =
      "estimator." + _params.GetParam("estimator", std::string());
  if (ALL_CONFIGS.find(estimator_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << estimator_name;
    _cv_stitcher->setEstimator(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::Estimator>, ImageStitcher *>
                         *>(ALL_CONFIGS.at(estimator_name).get())
            ->call(this));
  }

  // 图像特征点提取器
  auto features_finder_name =
      "featuresFinder." + _params.GetParam("featuresFinder", std::string());
  if (ALL_CONFIGS.find(features_finder_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << features_finder_name;
    _cv_stitcher->setFeaturesFinder(
        dynamic_cast<CallAble<cv::Ptr<cv::FeatureDetector>, ImageStitcher *> *>(
            ALL_CONFIGS.at(features_finder_name).get())
            ->call(this));
  }

  // 特征匹配器
  auto features_matcher_name =
      "featuresMatcher." + _params.GetParam("featuresMatcher", std::string());
  if (ALL_CONFIGS.find(features_matcher_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << features_matcher_name;
    _cv_stitcher->setFeaturesMatcher(
        dynamic_cast<
            CallAble<cv::Ptr<cv::detail::FeaturesMatcher>, ImageStitcher *> *>(
            ALL_CONFIGS.at(features_matcher_name).get())
            ->call(this));
  }

  // 设置投影类型
  auto warper_name = "warper." + _params.GetParam("warper", std::string());
  if (ALL_CONFIGS.find(warper_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << warper_name;
    _cv_stitcher->setWarper(
        dynamic_cast<CallAble<cv::Ptr<cv::WarperCreator>, ImageStitcher *> *>(
            ALL_CONFIGS.at(warper_name).get())
            ->call(this));
  }

  // 接缝查找器
  auto seam_finder_name =
      "seamFinder." + _params.GetParam("seamFinder", std::string());
  if (ALL_CONFIGS.find(seam_finder_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << seam_finder_name;
    _cv_stitcher->setSeamFinder(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::SeamFinder>, ImageStitcher *>
                         *>(ALL_CONFIGS.at(seam_finder_name).get())
            ->call(this));
  }

  // 光照补偿
  auto exposure_compensator_name =
      "exposureCompensator." +
      _params.GetParam("exposureCompensator", std::string());
  if (ALL_CONFIGS.find(exposure_compensator_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << exposure_compensator_name;
    _cv_stitcher->setExposureCompensator(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::ExposureCompensator>,
                              ImageStitcher *> *>(
            ALL_CONFIGS.at(exposure_compensator_name).get())
            ->call(this));
  }

  // 联合绑定优化
  auto bundle_adjuster_name =
      "bundleAdjuster." + _params.GetParam("bundleAdjuster", std::string());
  if (ALL_CONFIGS.find(bundle_adjuster_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << bundle_adjuster_name;
    _cv_stitcher->setBundleAdjuster(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::BundleAdjusterBase>,
                              ImageStitcher *> *>(
            ALL_CONFIGS.at(bundle_adjuster_name).get())
            ->call(this));
  }

  // 图像融合模式
  auto blender_name = "blender." + _params.GetParam("blender", std::string());
  if (ALL_CONFIGS.find(blender_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << blender_name;
    _cv_stitcher->setBlender(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::Blender>, ImageStitcher *> *>(
            ALL_CONFIGS.at(blender_name).get())
            ->call(this));
  }

  // 图像插值方式
  auto interpolation_flags_name =
      "interpolationFlags." +
      _params.GetParam("interpolationFlags", std::string());
  if (ALL_CONFIGS.find(interpolation_flags_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << interpolation_flags_name;
    _cv_stitcher->setInterpolationFlags(
        dynamic_cast<CallAble<cv::InterpolationFlags> *>(
            ALL_CONFIGS.at(interpolation_flags_name).get())
            ->call());
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
    _divide_images =
        dynamic_cast<CallAble<int> *>(ALL_CONFIGS.at(divide_image_name).get())
            ->call();
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
        "\"blender\": {\"value\": \"MultiBandBlender\"},"
        "\"bundleAdjuster\": {\"value\": \"BundleAdjusterAffine\"},"
        "\"estimator\": {\"value\": \"AffineBasedEstimator\"},"
        "\"exposureCompensator\": {\"value\": \"NoExposureCompensator\"},"
        "\"featuresFinder\": {\"value\": \"SURF\"},"
        "\"featuresMatcher\": {\"value\": \"AffineBestOf2NearestMatcher\"},"
        "\"interpolationFlags\": {\"value\": \"INTER_LINEAR\"},"
        "\"mode\": {\"value\": \"SCANS\"},"
        "\"seamFinder\": {\"value\": \"GraphCutSeamFinder\"},"
        "\"warper\": {\"value\": \"AffineWarper\"}"
        "}");
  }
}

auto ImageStitcher::Clean() -> bool {
  _images.clear();
  _images_features.clear();
  _pairwise_matches.clear();
  _camera_params.clear();
  return true;
}

auto ImageStitcher::SetImages(std::vector<ImagePtr> images) -> bool {
  _images = images;
  return true;
}

auto ImageStitcher::SetImages(std::vector<std::string> image_files) -> bool {
  _images.resize(image_files.size());
  _image_exifs.resize(image_files.size());
  for (int i = 0; i < image_files.size(); ++i) {
    if (!std::filesystem::exists(image_files[i])) {
      continue;
    }
    std::ifstream file;
    file.open(image_files[i], std::ios::binary);
    std::vector<uchar> buffer((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    _images[i] = new Image(cv::imdecode(buffer, cv::IMREAD_COLOR));
    easyexif::EXIFInfo result;
    result.parseFrom(buffer.data(), buffer.size());
    ImageExif image_exif;
    image_exif.camera_model = result.Model;
    image_exif.exposure_bias = result.ExposureBiasValue;
    image_exif.exposure_time = result.ExposureTime;
    image_exif.f_stop = result.FNumber;
    image_exif.flash_used = result.Flash;
    image_exif.focal_length = result.FocalLength;
    image_exif.focal_length_35mm = result.FocalLengthIn35mm;
    image_exif.gps_altitude = result.GeoLocation.Altitude;
    image_exif.gps_latitude = result.GeoLocation.Latitude;
    image_exif.gps_longitude = result.GeoLocation.Longitude;
    image_exif.image_height = result.ImageHeight;
    image_exif.image_width = result.ImageWidth;
    image_exif.image_orientation = result.Orientation;
    image_exif.iso_speed = result.ISOSpeedRatings;
    image_exif.original_date_time = result.DateTimeOriginal;
    _image_exifs[i] = image_exif;
  }
  return true;
}

auto ImageStitcher::Stitch(std::vector<Image> &images)
    -> std::vector<ImagePtr> {
  std::vector<ImagePtr> results;
  Image result;
  signal_run_message.notify("开始拼接", -1);
  auto status = _cv_stitcher->stitch(images, result);
  std::string str;
  if (status == cv::Stitcher::OK) {
    str = "已完成拼接:";
    for (int i : _cv_stitcher->component()) {
      if (_divide_images == 1 || _divide_images == 2) {
        str +=
            std::to_string(i / 3 + 1) + "-" + std::to_string(i % 3 + 1) + ", ";
      } else {
        str += std::to_string(i + 1) + ", ";
      }
    }
    results.push_back(new Image(result));
    signal_run_message.notify(str, -1);
  } else {
    signal_run_message.notify("拼接失败,错误代码: " + std::to_string(status),
                              -1);
  }
  signal_result.notify(results);
  signal_run_progress.notify(1);
  return results;
}

auto ImageStitcher::IncrementalStitch(std::vector<Image> &images)
    -> std::vector<ImagePtr> {
  signal_run_message.notify("开始拼接", -1);
  _images_features.resize(images.size());
  _pairwise_matches.resize(images.size() - 1);
  _camera_params.resize(images.size());
  _images_features[0] = Features(images[0]);
  _images_features[0].img_idx = 0;
  for (int i = 1; i < images.size(); ++i) {
    signal_run_message("Detect features " + std::to_string(i) + "/" +
                           std::to_string(images.size() - 1),
                       -1);
    _images_features[i].img_idx = i;
    _images_features[i] = Features(images[i]);
    signal_run_message("matching features " + std::to_string(i) + "/" +
                           std::to_string(images.size() - 1),
                       -1);
    _pairwise_matches[i - 1] =
        Matcher(_images_features[i - 1], _images_features[i]);
    LOG(INFO) << "pairwise_matches size : " << _pairwise_matches[i - 1].size();
    signal_run_message("estimate camera params " + std::to_string(i) + "/" +
                           std::to_string(images.size() - 1),
                       -1);
    _camera_params[i] =
        EstimateCameraParams({_images_features[i - 1], _images_features[i]},
                             _pairwise_matches[i - 1]);

    signal_run_message("bundle adjuster " + std::to_string(i) + "/" +
                           std::to_string(images.size() - 1),
                       -1);
    _cv_stitcher->bundleAdjuster()->setConfThresh(
        _cv_stitcher->panoConfidenceThresh());
    if (!(*_cv_stitcher->bundleAdjuster())(
            {_images_features[i - 1], _images_features[i]},
            _pairwise_matches[i - 1], _camera_params[i])) {
      LOG(ERROR) << "Adjuster failed";
    }
    LOG(INFO) << "_camera_params size : " << _camera_params[i].size();
    LOG(INFO) << i << " - " << i + 1 << "\nE:\n"
              << _camera_params[i][0].R << "\n------------------------\n"
              << _camera_params[i][1].R;
    LOG(INFO) << "t\n"
              << _camera_params[i][0].t << "\n------------------------\n"
              << _camera_params[i][1].t;
    LOG(INFO) << "f : " << _camera_params[i][0].focal
              << "\nf : " << _camera_params[i][1].focal;
    LOG(INFO) << "K:\n"
              << _camera_params[i][0].K() << "\n-------------------------\n"
              << _camera_params[i][1].K();
  }

  std::vector<double> focals;
  for (size_t i = 0; i < _camera_params.size(); ++i) {
    focals.push_back(_camera_params[i][0].focal);
  }
  std::sort(focals.begin(), focals.end());

  float warped_image_scale_ = 0;
  if (focals.size() % 2 == 1) {
    warped_image_scale_ = static_cast<float>(focals[focals.size() / 2]);
  } else {
    warped_image_scale_ = static_cast<float>(focals[focals.size() / 2 - 1] +
                                             focals[focals.size() / 2]) *
                          0.5f;
  }

  float seam_scale = 0;
  seam_scale = (std::min)(1.0, std::sqrt(_cv_stitcher->seamEstimationResol() *
                                         1e6 / images[0].size().area()));
  std::vector<cv::UMat> seam_est_imgs;
  cv::UMat img;
  seam_est_imgs.resize(images.size());

  for (size_t i = 0; i < images.size(); ++i) {
    resize(images[i], img, cv::Size(), seam_scale, seam_scale,
           cv::INTER_LINEAR_EXACT);
    seam_est_imgs[i] = img.clone();
  }

  std::vector<cv::Point> corners(images.size());
  std::vector<cv::UMat> masks_warped(images.size());
  std::vector<cv::UMat> images_warped(images.size());
  std::vector<cv::Size> sizes(images.size());
  std::vector<cv::UMat> masks(images.size());

  // Prepare image masks
  for (size_t i = 0; i < images.size(); ++i) {
    masks[i].create(seam_est_imgs[i].size(), CV_8U);
    masks[i].setTo(cv::Scalar::all(255));
  }

  cv::UMat pano;

  // Warp images and their masks
  cv::Ptr<cv::detail::RotationWarper> w =
      _cv_stitcher->warper()->create(float(warped_image_scale_));

  std::vector<CameraParams> camera_params;
  for (size_t i = 0; i < images.size(); ++i) {
    if (i == 0) {
      cv::Mat R_inv;
      cv::invert(_camera_params[i][0].R, R_inv);
      camera_params[i] = _camera_params[i][0];
      camera_params[i].R = _camera_params[i][1].R * R_inv;
    } else {
      cv::Mat R_inv;
      cv::invert(_camera_params[i][0].R, R_inv);
      camera_params[i] = _camera_params[i][0];
      camera_params[i].R =
          _camera_params[i][1].R * R_inv * camera_params[i - 1].R;
    }
    cv::Mat_<float> K;
    _camera_params[i][1].K().convertTo(K, CV_32F);
    K(0, 0) *= (float)1;
    K(0, 2) *= (float)1;
    K(1, 1) *= (float)1;
    K(1, 2) *= (float)1;

    corners[i] = w->warp(seam_est_imgs[i], K, camera_params[i].R,
                         _cv_stitcher->interpolationFlags(), cv::BORDER_REFLECT,
                         images_warped[i]);
    sizes[i] = images_warped[i].size();

    w->warp(masks[i], K, _camera_params[i][1].R, cv::INTER_NEAREST,
            cv::BORDER_CONSTANT, masks_warped[i]);
  }

  _cv_stitcher->exposureCompensator()->feed(corners, images_warped,
                                            masks_warped);
  for (size_t i = 0; i < images.size(); ++i) {
    _cv_stitcher->exposureCompensator()->apply(
        int(i), corners[i], images_warped[i], masks_warped[i]);
  }

  // Find seams
  std::vector<cv::UMat> images_warped_f(images.size());
  for (size_t i = 0; i < images.size(); ++i)
    images_warped[i].convertTo(images_warped_f[i], CV_32F);
  _cv_stitcher->seamFinder()->find(images_warped_f, corners, masks_warped);

  // Release unused memory
  seam_est_imgs.clear();
  images_warped.clear();
  images_warped_f.clear();
  masks.clear();
  img.release();

  cv::UMat img_warped, img_warped_s;
  cv::UMat dilated_mask, seam_mask, mask, mask_warped;

  cv::UMat full_img;
  float compose_scale = 0;
  for (size_t img_idx = 0; img_idx < images.size(); ++img_idx) {
    full_img = images[img_idx].getUMat(cv::ACCESS_MASK);

    if (_cv_stitcher->compositingResol() > 0) {
      compose_scale =
          (std::min)(1.0, std::sqrt(_cv_stitcher->compositingResol() * 1e6 /
                                    full_img.size().area()));
    }
    w = _cv_stitcher->warper()->create(compose_scale);

    std::vector<CameraParams> cameras_scaled(camera_params);

    // Update corners and sizes
    for (size_t i = 0; i < images.size(); ++i) {
      // Update intrinsics
      cameras_scaled[i].ppx *= compose_scale;
      cameras_scaled[i].ppy *= compose_scale;
      cameras_scaled[i].focal *= compose_scale;

      // Update corner and size
      cv::Size sz = images[i].size();
      if (std::abs(compose_scale - 1) > 1e-1) {
        sz.width = cvRound(images[i].size().width * compose_scale);
        sz.height = cvRound(images[i].size().height * compose_scale);
      }

      cv::Mat K;
      cameras_scaled[i].K().convertTo(K, CV_32F);
      cv::Rect roi = w->warpRoi(sz, K, cameras_scaled[i].R);
      corners[i] = roi.tl();
      sizes[i] = roi.size();
    }
    if (std::abs(compose_scale - 1) > 1e-1) {
      cv::resize(full_img, img, cv::Size(), compose_scale, compose_scale,
                 cv::INTER_LINEAR_EXACT);
    } else {
      img = full_img;
    }

    full_img.release();

    cv::Size img_size = img.size();
    cv::Mat K;
    cameras_scaled[img_idx].K().convertTo(K, CV_32F);
    w->warp(img, K, camera_params[img_idx].R,
            _cv_stitcher->interpolationFlags(), cv::BORDER_REFLECT, img_warped);

    mask.create(img_size, CV_8U);
    mask.setTo(cv::Scalar::all(255));
    w->warp(mask, K, camera_params[img_idx].R, cv::INTER_NEAREST,
            cv::BORDER_CONSTANT, mask_warped);

    _cv_stitcher->exposureCompensator()->apply((int)img_idx, corners[img_idx],
                                               img_warped, mask_warped);

    img_warped.convertTo(img_warped_s, CV_16S);
    img_warped.release();
    img.release();
    mask.release();

    // Make sure seam mask has proper size
    cv::dilate(masks_warped[img_idx], dilated_mask, cv::Mat());
    cv::resize(dilated_mask, seam_mask, mask_warped.size(), 0, 0,
               cv::INTER_LINEAR_EXACT);

    cv::bitwise_and(seam_mask, mask_warped, mask_warped);

    _cv_stitcher->blender()->prepare(corners, sizes);
    _cv_stitcher->blender()->feed(img_warped_s, mask_warped, corners[img_idx]);
    cv::UMat result, result_mask;
    _cv_stitcher->blender()->blend(result, result_mask);
    result.convertTo(pano, CV_8U);

    auto results = std::vector<ImagePtr>{
        new Image(pano.getMat(cv::AccessFlag::ACCESS_RW))};
    signal_result.notify(results);
    signal_run_progress.notify(1);

    return results;
  }
  signal_result.notify(std::vector<ImagePtr>());
  signal_run_progress.notify(1);
  return std::vector<ImagePtr>();
}

auto ImageStitcher::Stitch() -> std::vector<ImagePtr> {
  if (_cv_stitcher.empty()) {
    SetParams(Parameters());
  }
  if (_images.size() <= 0) {
    signal_run_message.notify("请提供图像", -1);
    signal_result.notify(std::vector<ImagePtr>());
    return std::vector<ImagePtr>();
  }
  signal_run_message.notify("预备拼接图像", -1);
  std::vector<Image> images_;
  for (int i = 0; i < _images.size(); ++i) {
    if (_divide_images == 1) {
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
      images_.push_back(_images[i]->clone());
    }
  }
  if (_mode == Mode::ALL) {
    return Stitch(images_);
  } else if (_mode == Mode::INCREMENTAL) {
    return IncrementalStitch(images_);
  }
  signal_run_message("未知拼接模式", 10000);
  signal_result(std::vector<ImagePtr>());
  signal_run_progress(1);
  return std::vector<ImagePtr>();
}

auto ImageStitcher::ImageSize() -> int { return _images.size(); }

auto ImageStitcher::Features(const Image &image) -> ImageFeatures {
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

auto ImageStitcher::Matcher(const ImageFeatures &features1,
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
  cv::drawMatches(image1, f1.keypoints, image2, f2.keypoints,
                  matches.getMatches(), image);
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