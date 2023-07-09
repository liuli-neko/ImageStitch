#include <glog/logging.h>
#include <gtest/gtest.h>

#include <string>

#include "../imageView.hpp"

namespace Test {
using namespace ImageStitch;

TEST(ImageViewTest, ViewWindowTest) {
  std::string imgPath = ".\\testData\\image1.jpg";
  QImage img;
  if (img.load(imgPath.c_str())) {
    ImageView *view = new ImageView();
    view->SetImage(img);
    view->setAttribute(Qt::WA_DeleteOnClose, true);
    view->show();
  } else {
    LOG(INFO) << "Image load error: " << imgPath;
  }
}

}  // namespace Test