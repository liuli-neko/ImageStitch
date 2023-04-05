#pragma once

#include <QComboBox>
#include <QDialog>
#include <QFormLayout>
#include <QPushButton>
#include <QScrollArea>
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
  void SetDafultText(const int index, const QString &text);
  void SetDafultIndex(const int index, const int &option_index);

 public Q_SLOTS:
  void accept() override;
  void reject() override;

 Q_SIGNALS:
  void accepted(QVector<QPair<QString, QString>> settings);

 private:
  QScrollArea *scroll_area;
  QVector<QComboBox *> config_box;
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
  void ConfigurationStitcher(QVector<QPair<QString, QString>> settings);

 public:
  class _Solts : public Trackable {
   public:
    explicit _Solts(ImageStitcherView *parent) : parent(parent) {}
    void SetProgress(const float progress);
    void ShowMessage(const std::string &message, int timeout = 0);
    void Result(ImagePtr img);

   private:
    ImageStitcherView *parent;
    ImagePtr current_result;
    QString current_message;
    float current_progress;
  } _solts = _Solts(this);

 private:
  ImageItemModel *m_model;
  ImageBox *input_image_box;
  ImageView *result_image_view;
  QPushButton *stitch_button;
  QPushButton *config_button;
  ImageStitcher image_stitcher;
  std::future<void> task;
};

}  // namespace ImageStitch