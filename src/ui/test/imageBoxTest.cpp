#include <glog/logging.h>
#include <gtest/gtest.h>

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include "../imageBox.hpp"

namespace Test {
using namespace ImageStitch;
TEST(ImageBoxTest, BoxViewTest) {
  QWidget* widget = new QWidget();
  ImageBox* box = new ImageBox(widget);
  ImageBox* box2 = new ImageBox(widget);
  ImageBox* box3 = new ImageBox(widget);
  ImageItemModel* model = new ImageItemModel(box);
  ImageItemModel* model2 = new ImageItemModel(box2);
  ImageItemModel* model3 = new ImageItemModel(box3);
  model->addItem(
      "E:\\workplace\\qt_project\\ImageStitch\\src\\gtest\\testData\\image1."
      "jpg");
  model->addItem(
      "E:\\workplace\\qt_project\\ImageStitch\\src\\gtest\\testData\\image2."
      "jpg");
  model->addItem(
      "E:\\workplace\\qt_project\\ImageStitch\\src\\gtest\\testData\\test_"
      "image_1.jpg");
  box->setModel(model);
  box2->setModel(model2);
  box3->setModel(model3);
  box->setGeometry({0, 10, 250, 300});
  box2->setGeometry({300, 10, 250, 300});
  box3->setGeometry({600, 10, 250, 300});
  widget->resize(800, 600);
  widget->setAttribute(Qt::WA_DeleteOnClose, true);
  widget->show();
}
}  // namespace Test