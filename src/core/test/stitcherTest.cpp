#include <glog/logging.h>
#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>

#include "../imageStitcher/imageStitcher.hpp"

namespace Test {
using namespace ImageStitch;

class StatusShow : public Trackable {
 public:
  void print(const std::string &message, int timeout) {
    std::cout << message << std::endl;
  }
  void print1(const std::string &message) { std::cout << message << std::endl; }
  void showImage(ImagePtr img) {
    double scale = (std::min)(1400.0 / img->cols, 1400.0 / img->rows);
    cv::Mat resized;
    cv::resize(*img, resized, cv::Size(), scale, scale);
    cv::imshow("result", resized);
    cv::waitKey();
  }
};

TEST(ImageStitcherTest, CvStitcherTest) {
  auto imageStitcher = ImageStitcher();
  Image image1 = ImageLoad(
      "E:\\workplace\\qt_"
      "project\\ImageStitch\\src\\gtest\\testData\\indoorScene1.jpg");
  Image image2 = ImageLoad(
      "E:\\workplace\\qt_"
      "project\\ImageStitch\\src\\gtest\\testData\\indoorScene2.jpg");
  Image image3 = ImageLoad(
      "E:\\workplace\\qt_"
      "project\\ImageStitch\\src\\gtest\\testData\\indoorScene3.jpg");
  // Image image4 = ImageLoad(
  //     "E:\\workplace\\qt_"
  //     "project\\ImageStitch\\src\\gtest\\testData\\image1.jpg");
  // Image image5 = ImageLoad(
  //     "E:\\workplace\\qt_"
  //     "project\\ImageStitch\\src\\gtest\\testData\\image2.jpg");
  imageStitcher.AddImage(ImagePtr(new Image(image3)));
  imageStitcher.AddImage(ImagePtr(new Image(image1)));
  imageStitcher.AddImage(ImagePtr(new Image(image2)));
  // imageStitcher.AddImage(ImagePtr(new Image(image4)));
  // imageStitcher.AddImage(ImagePtr(new Image(image5)));

  LOG(INFO) << "start Stitch image";
  StatusShow show;
  imageStitcher.signal_run_message.connect(&StatusShow::print, &show);
  imageStitcher.signal_result.connect(&StatusShow::showImage, &show);

  Parameters params;
  params.SetParam("mode", "PANORAMA");
  // params.SetParam("estimator", "HomographyBasedEstimator");
  params.SetParam("featuresFinder", "SIFT");
  params.SetParam("featuresMatcher", "BestOf2NearestRangeMatcher");
  params.SetParam("warper", "PlaneWarper");
  // params.SetParam("seamFinder", "DpSeamFinder");
  // params.SetParam("exposureCompensator", "BlocksChannelsCompensator");
  // params.SetParam("bundleAdjuster", "NoBundleAdjuster");
  params.SetParam("blender", "MULTI_BAND");
  params.SetParam("interpolationFlags", "INTER_CUBIC");
  imageStitcher.SetParams(params);
  auto stitchedImage = imageStitcher.Stitch();

  LOG(INFO) << "Stitched image";
  // ImageView *view = new ImageView();
  // view->SetImage(cv2qt::CvMat2QImage(*stitchedImage));
  // view->setAttribute(Qt::WA_DeleteOnClose, true);
  // view->show();
}
}  // namespace Test
