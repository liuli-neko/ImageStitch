#include "imageView.hpp"

#include <glog/logging.h>

#include <QFileDialog>
#include <QMenu>
#include <cmath>

namespace ImageStitch {
ImageView::ImageView(QWidget *parent) : QWidget(parent) {
  setupUi(kDefaultWindowWidth, kDefaultWindowHeight);
  _pixmap = QPixmap();
  _pixmap.fill(Qt::transparent);
}

ImageView::ImageView(const QImage &image, QWidget *parent) : QWidget(parent) {
  setupUi(kDefaultWindowWidth, kDefaultWindowHeight);
  SetImage(image);
}
ImageView::ImageView(const QPixmap &pixmap, QWidget *parent) : QWidget(parent) {
  setupUi(kDefaultWindowWidth, kDefaultWindowHeight);
  SetPixmap(pixmap);
}

void ImageView::setupUi(int iWidth, int iHeight) {
  setWindowTitle(tr("图像"));
  resize(iWidth, iHeight);
  _iXOffset = 0;
  _iYOffset = 0;
  _iScale = 1.0;
  _mouseLeftPressed = false;
  _textLabel = new QLabel(this);
  _textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  _textLabel->setAlignment(Qt::AlignCenter);
  setContextMenuPolicy(Qt::CustomContextMenu);
  QWidget::connect(this, &ImageView::customContextMenuRequested, this,
                   &ImageView::Menu);
}
ImageView::~ImageView() {
  // TODO(llhsdmd): to do
}
void ImageView::Menu(const QPoint &pos) {
  QMenu menu;
  if (!_pixmap.isNull()) {
    menu.addAction(tr("顺时针旋转90°"), [this]() {
      QMatrix leftmatrix;
      leftmatrix.rotate(90);
      QPixmap pix =
          GetPixmap().transformed(leftmatrix, Qt::SmoothTransformation);
      SetPixmap(pix);
    });
    menu.addAction(tr("逆时针旋转90°"), [this]() {
      QMatrix leftmatrix;
      leftmatrix.rotate(270);
      QPixmap pix =
          GetPixmap().transformed(leftmatrix, Qt::SmoothTransformation);
      SetPixmap(pix);
    });
    menu.addAction(tr("水平翻转"), [this]() {
      SetPixmap(
          QPixmap::fromImage(GetPixmap().toImage().mirrored(true, false)));
    });
    menu.addAction(tr("竖直翻转"), [this]() {
      SetPixmap(
          QPixmap::fromImage(GetPixmap().toImage().mirrored(false, true)));
    });
    menu.addSeparator();
    menu.addAction(tr("保存图像"), [this]() {
      QString fileName = QFileDialog::getSaveFileName(
          this, tr("保存图像"), tr("未命名"), tr("Images (*.png *.jpg)"));
      if (!fileName.isEmpty()) {
        _pixmap.save(fileName);
      }
    });
    menu.exec(mapToGlobal(pos));
  }
}
void ImageView::AutoScale() {
  float scale_x = (float)width() / (float)_pixmap.width();
  float scale_y = (float)height() / (float)_pixmap.height();
  float scale = scale_x < scale_y ? scale_x : scale_y;
  SetOffset((width() - _pixmap.width() * scale) / 2,
            (height() - _pixmap.height() * scale) / 2);
  SetScale(scale);
}
void ImageView::ShowMessage(const QString &message) {
  if (_textLabel != nullptr) {
    _textLabel->setText(message);
  }
}

void ImageView::SetImage(const QImage &image) {
  if (!image.isNull()) {
    _pixmap = QPixmap::fromImage(image);
    AutoScale();
    _textLabel->close();
  }
}

void ImageView::SetPixmap(const QPixmap &pixmap) {
  if (!pixmap.isNull()) {
    _pixmap = pixmap;
    AutoScale();
    _textLabel->close();
  }
}

// Member Funcitons
void ImageView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    _moveBeginPose[0] = event->x();
    _moveBeginPose[1] = event->y();
    _mouseLeftPressed = true;
  }
  QWidget::mousePressEvent(event);
}
void ImageView::mouseMoveEvent(QMouseEvent *event) {
  if (_mouseLeftPressed) {
    _moveEndPose[0] = event->x();
    _moveEndPose[1] = event->y();
    Offset(_moveEndPose[0] - _moveBeginPose[0],
           _moveEndPose[1] - _moveBeginPose[1]);
    _moveBeginPose[0] = _moveEndPose[0];
    _moveBeginPose[1] = _moveEndPose[1];
  }
  QWidget::mouseMoveEvent(event);
}
void ImageView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    _moveEndPose[0] = event->x();
    _moveEndPose[1] = event->y();
    if (_mouseLeftPressed) {
      Offset(_moveBeginPose[0] - _moveEndPose[0],
             _moveBeginPose[1] - _moveEndPose[1]);
      _mouseLeftPressed = false;
    }
  }
  QWidget::mouseReleaseEvent(event);
}
void ImageView::SetOffset(int x, int y) {
  if (x != _iXOffset || y != _iYOffset) {
    _iXOffset = x;
    _iYOffset = y;
    update();
  }
}
void ImageView::Offset(int x, int y) {
  SetOffset(_iXOffset + x, _iYOffset + y);
}
void ImageView::SetScale(float scale) {
  if (scale != _iScale && scale > 0) {
    _iScale = scale;
    update();
  }
}
void ImageView::Scale(float scale) { SetScale(_iScale * scale); }

void ImageView::resizeEvent(QResizeEvent *event) {
  if (_textLabel != nullptr) {
    _textLabel->resize(event->size());
  }
  AutoScale();
  QWidget::resizeEvent(event);
}

void ImageView::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  QPoint start{-_iXOffset, -_iYOffset};
  QPoint size{this->width(), this->height()};
  int left = 0, up = 0;
  if (start.x() < 0) {
    left = -start.x();
  }
  if (start.y() < 0) {
    up = -start.y();
  }
  QRect target{left, up, (size.x() - left), (size.y() - up)};
  QRect source{(start + QPoint{left, up}) / _iScale, (size + start) / _iScale};
  painter.drawPixmap(target, _pixmap, source);
  // painter.setPen(QColor("blue"));
  // painter.drawRect(0, 0, size.x() - 1, size.y() - 1);
  QWidget::paintEvent(event);
}
void ImageView::wheelEvent(QWheelEvent *event) {
  // 获取鼠标中键滚动量，正负表方向
  float delta = ((float)event->y() / event->delta());
  // 基础缩放系数1.05，缩放次数为滚动量。
  float scale_coeff = powf(1.05, delta);
  if (_iScale * scale_coeff * _pixmap.width() >= kMaxImageWidth ||
      _iScale * scale_coeff * _pixmap.height() >= kMaxImageHeight) {
    scale_coeff = (kMaxImageWidth / _pixmap.width()) / _iScale;
    scale_coeff =
        (std::min)(scale_coeff, (kMaxImageHeight / _pixmap.height()) / _iScale);
  }
  // 计算鼠标所在点变换后的偏移量
  QPoint pos = event->pos();
  QPoint origin_offset{pos.x() - _iXOffset, pos.y() - _iYOffset};
  origin_offset *= scale_coeff;
  SetOffset(pos.x() - origin_offset.x(), pos.y() - origin_offset.y());
  Scale(scale_coeff);
}
QSize ImageView::sizeHint() const { return _pixmap.size(); }

}  // namespace ImageStitch