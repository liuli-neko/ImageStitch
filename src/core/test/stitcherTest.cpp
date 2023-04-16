#include <glog/logging.h>
#include <gtest/gtest.h>

#include <QGridLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <string>

#include "../../ui/customizeTitleWidget.hpp"
#include "../../ui/imageView.hpp"
#include "../imageStitcher/imageStitcher.hpp"
#include "../qtCommon/cv2qt.hpp"

namespace Test {
using namespace ImageStitch;

class StatusShow : public Trackable {
 public:
  void print(const std::string &message, int timeout) {
    std::cout << message << std::endl;
  }
  void print1(const std::string &message) { std::cout << message << std::endl; }
  void showImage(ImagePtr img) {
    LOG(INFO) << "Stitched image";
    ImageView *view = new ImageView();
    view->SetImage(cv2qt::CvMat2QImage(*img));
    view->setAttribute(Qt::WA_DeleteOnClose, true);
    view->show();
  }
};

TEST(ImageStitcherTest, CvStitcherTest) {
  std::shared_ptr<ImageStitcher> imageStitcher(new ImageStitcher());
  std::filesystem::path path = "E:\\download\\files\\20220803\\3";
  std::vector<std::string> files;
  for (auto &file : std::filesystem::directory_iterator(path)) {
    if (file.is_regular_file()) {
      auto file_path = file.path();
      std::cout << file_path << std::endl;
      files.push_back(file_path.string());
    }
  }
  imageStitcher->SetImages(files);

  LOG(INFO) << "start Stitch image";
  StatusShow show;
  imageStitcher->signal_run_message.connect(&StatusShow::print, &show);
  // imageStitcher->signal_result.connect(&StatusShow::showImage, &show);

  Parameters params;
  params.SetParam("mode", "PANORAMA");
  params.SetParam("estimator", "HomographyBasedEstimator");
  params.SetParam("featuresFinder", "SIFT");
  params.SetParam("featuresMatcher", "BestOf2NearestRangeMatcher");
  params.SetParam("warper", "PlaneWarper");
  params.SetParam("seamFinder", "DpSeamFinder");
  params.SetParam("exposureCompensator", "BlocksChannelsCompensator");
  params.SetParam("bundleAdjuster", "BundleAdjusterRay");
  params.SetParam("blender", "MULTI_BAND");
  params.SetParam("interpolationFlags", "INTER_CUBIC");
  imageStitcher->SetParams(params);

  LOG(INFO) << "Stitched image";
  CustomizeTitleWidget *window = new CustomizeTitleWidget();
  QWidget *widget = new QWidget();
  window->setCentralWidget(widget);

  QGridLayout *layout = new QGridLayout();

  ImageView *view1 = new ImageView();
  ImageView *view2 = new ImageView();
  ImageView *view3 = new ImageView();

  QValidator *validator = new QIntValidator(0, imageStitcher->ImageSize());
  QLineEdit *feature_match1 = new QLineEdit();
  QLineEdit *feature_match2 = new QLineEdit();
  QPushButton *feature_match = new QPushButton();
  feature_match1->setValidator(validator);
  feature_match2->setValidator(validator);
  feature_match->setText("show match");
  QWidget::connect(
      feature_match, &QPushButton::clicked,
      [imageStitcher, feature_match1, feature_match2, view1, view2,
       view3](bool checked) {
        if (!feature_match1->text().isEmpty() &&
            !feature_match2->text().isEmpty()) {
          bool ok1, ok2;
          int a = feature_match1->text().toInt(&ok1),
              b = feature_match2->text().toInt(&ok2);

          if (ok1 && ok2) {
            imageStitcher->Features(a);
            imageStitcher->Features(b);
            imageStitcher->Matcher(a, b);
            view1->SetImage(
                cv2qt::CvMat2QImage(imageStitcher->DrawKeypoint(a)));
            view2->SetImage(
                cv2qt::CvMat2QImage(imageStitcher->DrawKeypoint(b)));
            view3->SetImage(
                cv2qt::CvMat2QImage(imageStitcher->DrawMatches(a, b)));
          }
        }
      });
  view1->setMinimumSize(100, 100);
  view2->setMinimumSize(100, 100);
  view3->setMinimumSize(100, 100);

  layout->addWidget(view1, 0, 0, 1, 1);
  layout->addWidget(view2, 0, 1, 1, 1);
  layout->addWidget(view3, 1, 0, 1, 2);
  layout->addWidget(feature_match1, 2, 0, 1, 1);
  layout->addWidget(feature_match2, 2, 1, 1, 1);
  layout->addWidget(feature_match, 3, 0, 1, 2);

  widget->setLayout(layout);

  window->setAttribute(Qt::WA_DeleteOnClose, true);
  window->show();
}
}  // namespace Test
