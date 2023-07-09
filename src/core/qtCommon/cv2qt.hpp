#pragma once

#include <QImage>
#include <opencv2/opencv.hpp>

namespace cv2qt {
QImage CvMat2QImage(const cv::Mat& cvImage);
cv::Mat QImage2CvMat(const QImage& image);
}