#pragma once

// #include <QAbstractItemView>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

#include "customizeTitleWidget.hpp"

namespace ImageStitch {
class ImageView : public QWidget {
  Q_OBJECT
 public:
  const int32_t kMaxImageWidth = ~(1 << 31);
  const int32_t kMaxImageHeight = ~(1 << 31);
  const int32_t kDefaultWindowWidth = 800;
  const int32_t kDefaultWindowHeight = 600;

 public:
  ImageView(const QImage &image, QWidget *parent = nullptr);
  ImageView(const QPixmap &pixmap, QWidget *parent = nullptr);
  ImageView(QWidget *parent = nullptr);
  ~ImageView();

  void SetImage(const QImage &image);
  void SetPixmap(const QPixmap &pixmap);
  void ShowMessage(const QString &message);
  void SetOffset(int x, int y);
  void Offset(int x, int y);
  void SetScale(float scale);
  void Scale(float scale);
  void AutoScale();
  void Menu(const QPoint &pos);
  inline const QPixmap &GetPixmap() const { return _pixmap; }
  inline QPixmap GetPixmap() { return _pixmap; }

 protected:
  // Member Funcitons
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  QSize sizeHint() const override;
  void setupUi(int iWidth, int iHeight);

 private:
  QPixmap _pixmap;
  QLabel *_textLabel;
  int _iXOffset, _iYOffset;
  float _iScale;
  bool _mouseLeftPressed;
  int _moveBeginPose[2], _moveEndPose[2];
};
}  // namespace ImageStitch