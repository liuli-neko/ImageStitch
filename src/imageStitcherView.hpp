#pragma once

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>
#include <QSplitter>
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

class ImageStitcherView : public QWidget {
  Q_OBJECT
 public:
  ImageStitcherView(QWidget *parent = nullptr);
  void SetupUi(const int width, const int height);
  void Stitch();
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
  std::future<std::vector<ImagePtr>> task;
};

}  // namespace ImageStitch