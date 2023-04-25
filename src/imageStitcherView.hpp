#pragma once

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QWidget>
#include <future>

#include "core/imageStitcher/imageStitcher.hpp"
#include "ui/customizeTitleWidget.hpp"
#include "ui/imageBox.hpp"
#include "ui/imageView.hpp"

namespace ImageStitch {
class ConfigDialog : public QDialog {
  Q_OBJECT
 public:
  ConfigDialog(QWidget *parent = nullptr);
  void AddItem(const QString &title, const QVector<QString> &item,
               const QString &description = "");
  void AddItem(const QString &title, const double &min_value,
               const double &max_value, const QString &description = "",
               const QString &prefix = "", const QString &suffix = "");
  void AddItem(const QString &title, const int &min_value, const int &max_value,
               const QString &description = "", const QString &prefix = "",
               const QString &suffix = "");
  void SetDafultValue(const int index, const QString &text);
  void SetDafultValue(const int index, const double value);
  void SetDafultValue(const int index, const int value);

 public Q_SLOTS:
  void accept() override;
  void reject() override;

 Q_SIGNALS:
  void accepted(QVector<QPair<QString, QVariant>> settings);

 private:
  QScrollArea *scroll_area;
  QVector<QWidget *> config_box;
  QFormLayout *form_layout;
  QWidget *view;
  QPushButton *confirm_button;
  QPushButton *cancel_button;
};

class MidDataView : public QWidget {
 public:
  MidDataView(ImageStitcher *image_stitcher, QWidget *parent = nullptr);
  void SetupUi();
  QImage GetImage(const int index);
  QImage GetFeaturesImage(const int index);
  QImage GetMatchesImage(const int index1, const int index2);
  QImage GetWarpImage(const int index1, const int index2);
  QString GetFeaturesText(const int index);
  QString GetMatchesText(const int index1, const int index2);
  QString GetCameraParamsText(const int index);
  CameraParams GetCameraParams(const int index);
  std::vector<QImage> GetCompensatorImage(const int index);
  QImage GetSeamMaskImage(const int index);

 private:
  ImageStitcher *image_stitcher;
  QLabel *label;
  std::vector<QImage> images;
  std::vector<QImage> features_images;
  std::vector<std::vector<QImage>> matches_images;
  std::vector<std::vector<QImage>> compensator_images;
  std::vector<QImage> seam_mask_images;
  std::vector<CameraParams> camera_params;
  QGridLayout *grid_layout;
  std::vector<QPushButton *> features_buttons;
  std::vector<QPushButton *> original_buttons;
  std::vector<std::vector<QPushButton *>> matches_buttons;
  std::vector<QPushButton *> compensator_buttons;
  std::vector<QPushButton *> seam_mask_buttons;
};

class ImageStitcherView : public QWidget {
  Q_OBJECT
 public:
  ImageStitcherView(QWidget *parent = nullptr);
  ~ImageStitcherView();
  void SetupUi(const int width, const int height);
  void CreateFromDirectory(const QString &path);
  void ShowImage(const QPixmap &pixmap);
  void ShowImage(const QImage &image);
  void ShowMessage(const QString &message);
  inline Parameters &Params() { return image_stitcher.GetParams(); }
  inline void SetParams(const Parameters &params) {
    image_stitcher.SetParams(params);
  }

 public:
  void paintEvent(QPaintEvent *event) override;

 Q_SIGNALS:
  void Message(const QString &message, int timeout = 0);
  void StitchProgress(const float progress);

 public slots:
  void Stitch(bool checked);
  void ShowMidData(bool checked);
  void ConfigurationStitcher(QVector<QPair<QString, QVariant>> settings);
  void ShowNextResults();
  void ShowPreResults();

 public:
  class _Solts : public Trackable {
   public:
    explicit _Solts(ImageStitcherView *parent) : parent(parent) {}
    void SetProgress(const float progress);
    void ShowMessage(const std::string &message, int timeout = 0);
    void Result(std::vector<ImagePtr> img);

   protected:
    std::vector<QImage> images;
    ImageStitcherView *parent;
    QString current_message;
    int current_index;
    float current_progress;

    friend class ImageStitcherView;
  } _solts = _Solts(this);

 protected:
  void StartStitcher();
  void EndStitcher(int result);

 private:
  ImageItemModel *m_model;
  ImageBox *input_image_box;
  ImageView *result_view_widget;
  QPushButton *stitch_button;
  QPushButton *config_button;
  QPushButton *next_result;
  QPushButton *pre_result;
  QPushButton *clean_images;
  ImageStitcher image_stitcher;
  QStatusBar *statusbar;
  std::thread task;
};

}  // namespace ImageStitch