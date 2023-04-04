#include "cv2qt.hpp"

#include <QDebug>
#include <QPainter>
#include <filesystem>
#include <sstream>
#include <vector>

namespace cv2qt {
QImage CvMat2QImage(const cv::Mat &cvImage) {
  if (cvImage.empty()) {
    return QImage();
  }

#ifdef WIN
  auto tmp_path = std::filesystem::temp_directory_path().string();
  std::stringstream tmp_filename;
  tmp_filename << tmp_path << "/" << std::hex << (rand() % 1000000)
               << (rand() % 1000000) << ".jpg";
  qDebug() << "temp file : " << tmp_filename.str().c_str();
  cv::imwrite(tmp_filename.str(), cvImage);
  return QImage(tmp_filename.str().c_str());
#endif

  QImage image;
  switch (cvImage.type()) {
    case CV_8UC1: {
      image = QImage((const uchar *)(cvImage.data), cvImage.cols, cvImage.rows,
                     cvImage.step, QImage::Format_Grayscale8);
      return image.copy();
    }
    case CV_8UC3: {
      // Copy input cv::Mat
      const uchar *pSrc = (const uchar *)cvImage.data;
      // Create QImage with same dimensions as input cv::Mat
      QImage image(pSrc, cvImage.cols, cvImage.rows, cvImage.step,
                   QImage::Format_RGB888);
      return image.rgbSwapped();
    }
    case CV_8UC4: {
      // Copy input cv::Mat
      const uchar *pSrc = (const uchar *)cvImage.data;
      // Create QImage with same dimensions as input cv::Mat
      QImage image(pSrc, cvImage.cols, cvImage.rows, cvImage.step,
                   QImage::Format_ARGB32);
      return image.copy();
    }
    default:
      cvImage.convertTo(cvImage, CV_8UC3);
      QImage image((const uchar *)cvImage.data, cvImage.cols, cvImage.rows,
                   cvImage.step, QImage::Format_RGB888);
      return image.rgbSwapped();
  }
}

cv::Mat QImage2CvMat(const QImage &image) {
// #ifdef WIN
//   auto tmp_path = std::filesystem::temp_directory_path().string();
//   std::stringstream tmp_filename;
//   tmp_filename << tmp_path << "/" << std::hex << (rand() % 1000000)
//                << (rand() % 1000000) << ".jpg";
//   image.save(tmp_filename.str().c_str());
//   qDebug() << "temp file : " << tmp_filename.str().c_str();
//   return cv::imread(tmp_filename.str());
// #endif
  cv::Mat mat;
  switch (image.format()) {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32_Premultiplied:
      mat = cv::Mat(image.height(), image.width(), CV_8UC4,
                    (void *)image.constBits(), image.bytesPerLine());
      cv::cvtColor(mat, mat, cv::COLOR_RGBA2RGB);
      break;
    case QImage::Format_RGB888:
      mat = cv::Mat(image.height(), image.width(), CV_8UC3,
                    (void *)image.constBits(), image.bytesPerLine());
      break;
    case QImage::Format_Indexed8:
      mat = cv::Mat(image.height(), image.width(), CV_8UC1,
                    (void *)image.constBits(), image.bytesPerLine());
      break;
  }
  return mat.clone();
}
}  // namespace cv2qt