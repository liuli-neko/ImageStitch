#pragma once

#include <glog/logging.h>

#include <filesystem>
#include <opencv2/opencv.hpp>
#include <string>

namespace ImageStitch {

template <typename T>
using Ptr = cv::Ptr<T>;
using Image = cv::Mat;
using ImagePtr = Ptr<Image>;
using Mat = cv::Mat;
using MatPtr = Ptr<Mat>;
using Feature2D = cv::Feature2D;
using KeyPoint = cv::KeyPoint;
using ImageFeatures = cv::detail::ImageFeatures;
using KeyPoints = std::vector<cv::KeyPoint>;
using Descriptors = cv::UMat;
using MatchesInfo = cv::detail::MatchesInfo;

inline Image ImageLoad(const std::string& file_path) {
  if (!std::filesystem::exists(file_path)) {
    LOG(WARNING) << "file : " << file_path << " not exists!" << std::endl;
    return Image();
  }
  return cv::imread(file_path, cv::IMREAD_COLOR);
}
}  // namespace ImageStitch