#include "imageStitcher.hpp"

#include <glog/logging.h>

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

static std::map<std::string, std::shared_ptr<CallAbleBase>> ALL_CONFIGS;
static std::vector<ConfigItem> CONFIG_TABLE;
static void init() {
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;
  {
    ConfigItem item;
    item.title = "mode";
    item.description =
        "拼接模式，PANORAMA拼接模式为通用全景图模式。SCANS假设图像是通过扫描仪"
        "或其他扫描设备拍摄的平面的图。";
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
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "estimator";
    item.description = "图像相机参数推断器，一般通过单应性矩阵推断参数。";
    item.options.push_back("HomographyBasedEstimator");
    ALL_CONFIGS.insert(std::make_pair(
        "estimator.HomographyBasedEstimator",
        CreateCallAble(std::function<cv::Ptr<cv::detail::Estimator>()>([]() {
          return cv::Ptr<cv::detail::Estimator>(
              new cv::detail::HomographyBasedEstimator());
        }))));
    item.options.push_back("AffineBasedEstimator");
    ALL_CONFIGS.insert(std::make_pair(
        "estimator.AffineBasedEstimator",
        CreateCallAble(std::function<cv::Ptr<cv::detail::Estimator>()>([]() {
          return cv::Ptr<cv::detail::Estimator>(
              new cv::detail::AffineBasedEstimator());
        }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "featuresFinder";
    item.description =
        "图像特征点提取器，用于提取图像中具备可描述特征的点，主流特征提取算法有"
        "SIFT，SURF，ORB。";
    item.options.push_back("ORB");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.ORB",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::ORB::create());
        }))));
    item.options.push_back("SIFT");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.SIFT",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::SIFT::create());
        }))));
    item.options.push_back("SURF");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.SURF",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::xfeatures2d::SURF::create());
        }))));
    item.options.push_back("BRISK");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.BRISK",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::BRISK::create());
        }))));
    item.options.push_back("MSER");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.MSER",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::MSER::create());
        }))));
    item.options.push_back("FastFeatureDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.FastFeatureDetector",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(
              cv::FastFeatureDetector::create());
        }))));
    item.options.push_back("AgastFeatureDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.AgastFeatureDetector",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(
              cv::AgastFeatureDetector::create());
        }))));
    item.options.push_back("GFTTDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.GFTTDetector",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::GFTTDetector::create());
        }))));
    item.options.push_back("SimpleBlobDetector");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.SimpleBlobDetector",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::SimpleBlobDetector::create());
        }))));
    item.options.push_back("KAZE");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.KAZE",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::KAZE::create());
        }))));
    item.options.push_back("AKAZE");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresFinder.AKAZE",
        CreateCallAble(std::function<cv::Ptr<cv::FeatureDetector>()>([]() {
          return cv::Ptr<cv::FeatureDetector>(cv::AKAZE::create());
        }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "featuresMatcher";
    item.description =
        "特征匹配器，用于匹配两张图像中的特征点，主流匹配算法有。";
    item.options.push_back("BestOf2NearestMatcher");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresMatcher.BestOf2NearestMatcher",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::FeaturesMatcher>()>([]() {
              return cv::Ptr<cv::detail::FeaturesMatcher>(
                  new cv::detail::BestOf2NearestMatcher());
            }))));
    item.options.push_back("BestOf2NearestRangeMatcher");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresMatcher.BestOf2NearestRangeMatcher",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::FeaturesMatcher>()>([]() {
              return cv::Ptr<cv::detail::FeaturesMatcher>(
                  new cv::detail::BestOf2NearestRangeMatcher());
            }))));
    item.options.push_back("AffineBestOf2NearestMatcher");
    ALL_CONFIGS.insert(std::make_pair(
        "featuresMatcher.AffineBestOf2NearestMatcher",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::FeaturesMatcher>()>([]() {
              return cv::Ptr<cv::detail::FeaturesMatcher>(
                  new cv::detail::AffineBestOf2NearestMatcher());
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "warper";
    item.description =
        "投影类型，用于将图像投影到平面或球面上，主流投影类型有平面投影，柱面"
        "投影，球面投影等。";
    item.options.push_back("PlaneWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PlaneWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::PlaneWarper>(); }))));
    item.options.push_back("CylindricalWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CylindricalWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::CylindricalWarper>(); }))));
    item.options.push_back("AffineWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.AffineWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::AffineWarper>(); }))));
    item.options.push_back("FisheyeWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.FisheyeWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::FisheyeWarper>(); }))));
    item.options.push_back("StereographicWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.StereographicWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::StereographicWarper>(); }))));
    item.options.push_back("CompressedRectilinearWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CompressedRectilinearWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::CompressedRectilinearWarper>(); }))));
    item.options.push_back("CompressedRectilinearPortraitWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CompressedRectilinearPortraitWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>([]() {
          return cv::makePtr<cv::CompressedRectilinearPortraitWarper>();
        }))));
    item.options.push_back("SphericalWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.SphericalWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::SphericalWarper>(); }))));
    item.options.push_back("PaniniWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PaniniWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::PaniniWarper>(); }))));
    item.options.push_back("PaniniPortraitWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PaniniPortraitWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::PaniniPortraitWarper>(); }))));
    item.options.push_back("MercatorWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.MercatorWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::MercatorWarper>(); }))));
    item.options.push_back("TransverseMercatorWarper");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.TransverseMercatorWarper",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::TransverseMercatorWarper>(); }))));
#ifdef HAVE_OPENCV_CUDAWARPING
    item.options.push_back("PlaneWarperGpu");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.PlaneWarperGpu",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::PlaneWarperGpu>(); }))));
    item.options.push_back("CylindricalWarperGpu");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.CylindricalWarperGpu",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::CylindricalWarperGpu>(); }))));
    item.options.push_back("SphericalWarperGpu");
    ALL_CONFIGS.insert(std::make_pair(
        "warper.SphericalWarperGpu",
        CreateCallAble(std::function<cv::Ptr<cv::WarperCreator>()>(
            []() { return cv::makePtr<cv::SphericalWarperGpu>(); }))));
#endif
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "bundleAdjuster";
    item.description =
        "联合绑定优化器，在相机参数初步推断完成后，被选中的所有全景图图像会全"
        "部给他做一次参数优化。";
    item.options.push_back("NoBundleAdjuster");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.NoBundleAdjuster",
        CreateCallAble(std::function<cv::Ptr<cv::detail::BundleAdjusterBase>()>(
            []() { return cv::makePtr<cv::detail::NoBundleAdjuster>(); }))));
    item.options.push_back("BundleAdjusterReproj");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterReproj",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>()>([]() {
              return cv::makePtr<cv::detail::BundleAdjusterReproj>();
            }))));
    item.options.push_back("BundleAdjusterRay");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterRay",
        CreateCallAble(std::function<cv::Ptr<cv::detail::BundleAdjusterBase>()>(
            []() { return cv::makePtr<cv::detail::BundleAdjusterRay>(); }))));
    item.options.push_back("BundleAdjusterAffine");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterAffine",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>()>([]() {
              return cv::makePtr<cv::detail::BundleAdjusterAffine>();
            }))));
    item.options.push_back("BundleAdjusterAffinePartial");
    ALL_CONFIGS.insert(std::make_pair(
        "bundleAdjuster.BundleAdjusterAffinePartial",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::BundleAdjusterBase>()>([]() {
              return cv::makePtr<cv::detail::BundleAdjusterAffinePartial>();
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "blender";
    item.description =
        "图像融合器，用于将拼接后的图像进行融合，主流融合算法有多频段融合，羽"
        "化融合等。";
    item.options.push_back("NoBlender");
    ALL_CONFIGS.insert(std::make_pair(
        "blender.NoBlender",
        CreateCallAble(std::function<cv::Ptr<cv::detail::Blender>()>([]() {
          return cv::detail::Blender::createDefault(cv::detail::Blender::NO);
        }))));
    item.options.push_back("MultiBandBlender");
    ALL_CONFIGS.insert(std::make_pair(
        "blender.MultiBandBlender",
        CreateCallAble(std::function<cv::Ptr<cv::detail::Blender>()>([]() {
          return cv::detail::Blender::createDefault(
              cv::detail::Blender::MULTI_BAND);
        }))));
    item.options.push_back("FeatherBlender");
    ALL_CONFIGS.insert(std::make_pair(
        "blender.FeatherBlender",
        CreateCallAble(std::function<cv::Ptr<cv::detail::Blender>()>([]() {
          return cv::detail::Blender::createDefault(
              cv::detail::Blender::FEATHER);
        }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "seamFinder";
    item.description =
        "拼接缝合线查找器，用于查找拼接图像之间的缝合线，主流查找算法有暴力查"
        "找，动态规划查找，基于图像分割的查找等。";
    item.options.push_back("NoSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.NoSeamFinder",
        CreateCallAble(std::function<cv::Ptr<cv::detail::SeamFinder>()>(
            []() { return cv::makePtr<cv::detail::NoSeamFinder>(); }))));
    item.options.push_back("DpSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.DpSeamFinder",
        CreateCallAble(std::function<cv::Ptr<cv::detail::SeamFinder>()>(
            []() { return cv::makePtr<cv::detail::DpSeamFinder>(); }))));
    item.options.push_back("VoronoiSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.VoronoiSeamFinder",
        CreateCallAble(std::function<cv::Ptr<cv::detail::SeamFinder>()>(
            []() { return cv::makePtr<cv::detail::VoronoiSeamFinder>(); }))));
    item.options.push_back("GraphCutSeamFinder");
    ALL_CONFIGS.insert(std::make_pair(
        "seamFinder.GraphCutSeamFinder",
        CreateCallAble(std::function<cv::Ptr<cv::detail::SeamFinder>()>([]() {
          return cv::makePtr<cv::detail::GraphCutSeamFinder>(
              cv::detail::GraphCutSeamFinderBase::COST_COLOR);
        }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "exposureCompensator";
    item.description =
        "曝光补偿器，用于对拼接图像进行曝光补偿，主流曝光补偿算法有无补偿，增"
        "益补偿，通道补偿，块增益补偿，块通道补偿等。";
    item.options.push_back("NoExposureCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.NoExposureCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>()>([]() {
              return cv::makePtr<cv::detail::NoExposureCompensator>();
            }))));
    item.options.push_back("GainCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.GainCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>()>(
                []() { return cv::makePtr<cv::detail::GainCompensator>(); }))));
    item.options.push_back("ChannelsCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.ChannelsCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>()>([]() {
              return cv::makePtr<cv::detail::ChannelsCompensator>();
            }))));
    item.options.push_back("BlocksGainCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.BlocksGainCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>()>([]() {
              return cv::makePtr<cv::detail::BlocksGainCompensator>();
            }))));
    item.options.push_back("BlocksChannelsCompensator");
    ALL_CONFIGS.insert(std::make_pair(
        "exposureCompensator.BlocksChannelsCompensator",
        CreateCallAble(
            std::function<cv::Ptr<cv::detail::ExposureCompensator>()>([]() {
              return cv::makePtr<cv::detail::BlocksChannelsCompensator>();
            }))));
    CONFIG_TABLE.push_back(item);
  }
  {
    ConfigItem item;
    item.title = "interpolationFlags";
    item.description =
        "插值方式，用于图像缩放和旋转时的像素插值，主流插值方式有最近邻插值，"
        "双线性插值，双三次插值，面积插值，Lanczos插值等。";
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
    item.options.push_back("INTER_MAX");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.INTER_MAX",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::INTER_MAX; }))));
    item.options.push_back("WARP_FILL_OUTLIERS");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.WARP_FILL_OUTLIERS",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::WARP_FILL_OUTLIERS; }))));
    item.options.push_back("WARP_INVERSE_MAP");
    ALL_CONFIGS.insert(
        std::make_pair("interpolationFlags.WARP_INVERSE_MAP",
                       CreateCallAble(std::function<cv::InterpolationFlags()>(
                           []() { return cv::WARP_INVERSE_MAP; }))));
    CONFIG_TABLE.push_back(item);
  }
}
}  // namespace

auto ImageStitcher::ParamTable() -> std::vector<ConfigItem> {
  init();
  return CONFIG_TABLE;
}
auto ImageStitcher::SetParams(const Parameters &_params) -> void {
  if (!_params.Empty()) {
    this->params = _params;
  }
  // 拼接模式
  auto stitcher_mode = "mode." + params.GetParam("mode", std::string());
  if (stitcher_mode != current_stitcher_mode) {
    if (ALL_CONFIGS.find(stitcher_mode) != ALL_CONFIGS.end()) {
      LOG(INFO) << stitcher_mode;
      cv_stitcher = dynamic_cast<CallAble<cv::Ptr<cv::Stitcher>> *>(
                        ALL_CONFIGS.at(stitcher_mode).get())
                        ->call();
      current_stitcher_mode = stitcher_mode;
    } else {
      cv_stitcher = cv::Stitcher::create();
      current_stitcher_mode = "mode.PANORAMA";
    }
  }

  // 相机参数推断模型
  auto estimator_name =
      "estimator." + params.GetParam("estimator", std::string());
  if (ALL_CONFIGS.find(estimator_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << estimator_name;
    cv_stitcher->setEstimator(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::Estimator>> *>(
            ALL_CONFIGS.at(estimator_name).get())
            ->call());
  }

  // 图像特征点提取器
  auto features_finder_name =
      "featuresFinder." + params.GetParam("featuresFinder", std::string());
  if (ALL_CONFIGS.find(features_finder_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << features_finder_name;
    cv_stitcher->setFeaturesFinder(
        dynamic_cast<CallAble<cv::Ptr<cv::FeatureDetector>> *>(
            ALL_CONFIGS.at(features_finder_name).get())
            ->call());
  }

  // 特征匹配器
  auto features_matcher_name =
      "featuresMatcher." + params.GetParam("featuresMatcher", std::string());
  if (ALL_CONFIGS.find(features_matcher_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << features_matcher_name;
    cv_stitcher->setFeaturesMatcher(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::FeaturesMatcher>> *>(
            ALL_CONFIGS.at(features_matcher_name).get())
            ->call());
  }

  // 设置投影类型
  auto warper_name = "warper." + params.GetParam("warper", std::string());
  if (ALL_CONFIGS.find(warper_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << warper_name;
    cv_stitcher->setWarper(dynamic_cast<CallAble<cv::Ptr<cv::WarperCreator>> *>(
                               ALL_CONFIGS.at(warper_name).get())
                               ->call());
  }

  // 接缝查找器
  auto seam_finder_name =
      "seamFinder." + params.GetParam("seamFinder", std::string());
  if (ALL_CONFIGS.find(seam_finder_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << seam_finder_name;
    cv_stitcher->setSeamFinder(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::SeamFinder>> *>(
            ALL_CONFIGS.at(seam_finder_name).get())
            ->call());
  }

  // 光照补偿
  auto exposure_compensator_name =
      "exposureCompensator." +
      params.GetParam("exposureCompensator", std::string());
  if (ALL_CONFIGS.find(exposure_compensator_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << exposure_compensator_name;
    cv_stitcher->setExposureCompensator(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::ExposureCompensator>> *>(
            ALL_CONFIGS.at(exposure_compensator_name).get())
            ->call());
  }

  // 联合绑定优化
  auto bundle_adjuster_name =
      "bundleAdjuster." + params.GetParam("bundleAdjuster", std::string());
  if (ALL_CONFIGS.find(bundle_adjuster_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << bundle_adjuster_name;
    cv_stitcher->setBundleAdjuster(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::BundleAdjusterBase>> *>(
            ALL_CONFIGS.at(bundle_adjuster_name).get())
            ->call());
  }

  // 图像融合模式
  auto blender_name = "blender." + params.GetParam("blender", std::string());
  if (ALL_CONFIGS.find(blender_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << blender_name;
    cv_stitcher->setBlender(
        dynamic_cast<CallAble<cv::Ptr<cv::detail::Blender>> *>(
            ALL_CONFIGS.at(blender_name).get())
            ->call());
  }

  // 图像插值方式
  auto interpolation_flags_name =
      "interpolationFlags." +
      params.GetParam("interpolationFlags", std::string());
  if (ALL_CONFIGS.find(interpolation_flags_name) != ALL_CONFIGS.end()) {
    LOG(INFO) << interpolation_flags_name;
    cv_stitcher->setInterpolationFlags(
        dynamic_cast<CallAble<cv::InterpolationFlags> *>(
            ALL_CONFIGS.at(interpolation_flags_name).get())
            ->call());
  }

  signal_run_message.notify("配置已生效.", 3000);
}
ImageStitcher::ImageStitcher() {
  init();
  if (std::filesystem::exists("./configuration.json")) {
    params.Load("./configuration.json");
  } else {
    params.FromString(
        "{\"blender\":{\"value\":\"MultiBandBlender\"},\"bundleAdjuster\":{"
        "\"value\":\"BundleAdjusterRay\"},\"estimator\":{\"value\":"
        "\"HomographyBasedEstimator\"},\"exposureCompensator\":{\"value\":"
        "\"BlocksChannelsCompensator\"},\"featuresFinder\":{\"value\":\"ORB\"},"
        "\"featuresMatcher\":{\"value\":\"BestOf2NearestMatcher\"},"
        "\"interpolationFlags\":{\"value\":\"INTER_CUBIC\"},\"mode\":{"
        "\"value\":\"PANORAMA\"},\"seamFinder\":{\"value\":"
        "\"GraphCutSeamFinder\"},\"warper\":{\"value\":\"PaniniWarper\"}}");
  }
}
auto ImageStitcher::AddImage(ImagePtr image) -> bool {
  images.push_back(image);
  return true;
}
auto ImageStitcher::Stitch() -> ImagePtr {
  if (cv_stitcher.empty()) {
    SetParams(Parameters());
  }
  ImagePtr result;
  std::vector<Image> images1;
  for (int i = 0; i < images.size(); ++i) {
    images1.push_back(*images[i]);
  }
  signal_run_progress.notify(0.3);
  result = ImagePtr(new Image);
  cv::Stitcher::Status status = cv_stitcher->stitch(images1, *result);
  signal_run_progress.notify(1);
  switch (status) {
    case cv::Stitcher::Status::OK:
      if (images1.size() != cv_stitcher->component().size()) {
        std::stringstream str;
        str << "stitch OK.拼接的图像id: ";
        for (auto i : cv_stitcher->component()) {
          str << i << ", ";
        }
        signal_run_message.notify(str.str(), 20000);
      } else {
        signal_run_message.notify("stitch OK", 20000);
      }
      break;
    case cv::Stitcher::Status::ERR_NEED_MORE_IMGS:
      if (images1.size() != cv_stitcher->component().size()) {
        std::stringstream str;
        str << "stitch failed.匹配的图像id: ";
        for (auto i : cv_stitcher->component()) {
          str << i << ", ";
        }
        signal_run_message.notify(str.str(), 20000);
      } else {
        signal_run_message.notify("stitch failed, 需要更多的图片", 20000);
      }
      break;
    case cv::Stitcher::Status::ERR_HOMOGRAPHY_EST_FAIL:
      signal_run_message.notify(
          "stitch failed, "
          "无法推断图片参数，可能是图片之间重叠区域不够明确或太少",
          20000);
      break;
    case cv::Stitcher::Status::ERR_CAMERA_PARAMS_ADJUST_FAIL:
      signal_run_message.notify(
          "stitch failed, 图片相机异常参数，可尝试更换图片", 20000);
      break;
    default:
      signal_run_message.notify("stitch failed, 未知异常", 20000);
      break;
  }
  signal_result.notify(result);
  // TODO(llhsdmd):to do
  return result;
}

auto ImageStitcher::RemoveImage(int index) -> bool {
  if (index < 0 || index >= images.size()) {
    return false;
  }
  images.erase(images.begin() + index);
  return true;
}
auto ImageStitcher::RemoveAllImages() -> bool {
  images.clear();
  return true;
}
auto ImageStitcher::GetImage(int index) -> ImagePtr {
  if (index < 0 || index >= images.size()) {
    return nullptr;
  }
  return images[index];
}
auto ImageStitcher::GetImages() -> std::vector<ImagePtr> { return images; }
};  // namespace ImageStitch