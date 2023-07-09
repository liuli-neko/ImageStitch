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
  void showImage(std::vector<ImagePtr> img) {
    LOG(INFO) << "Stitched image";
    ImageView *view = new ImageView();
    view->SetImage(cv2qt::CvMat2QImage(*img[0]));
    view->setAttribute(Qt::WA_DeleteOnClose, true);
    view->show();
  }
};

TEST(ImageStitcherTest, CvStitcherTest) {
  std::shared_ptr<ImageStitcher> imageStitcher(new ImageStitcher());
  std::filesystem::path path = "./imgs";
  std::vector<std::string> files;
  for (auto &file : std::filesystem::directory_iterator(path)) {
    if (file.is_regular_file()) {
      auto file_path = file.path();
      files.push_back(file_path.string());
    }
  }
  std::sort(files.begin(), files.end());
  std::for_each(files.begin(), files.end(),
                [](std::string a) { std::cout << a << std::endl; });
  imageStitcher->SetImages(files);

  LOG(INFO) << "start Stitch image";
  StatusShow show;
  imageStitcher->signal_run_message.connect(&StatusShow::print, &show);
  imageStitcher->signal_result.connect(&StatusShow::showImage, &show);

  imageStitcher->Stitch();
  LOG(INFO) << "Stitched image";
}
}  // namespace Test
