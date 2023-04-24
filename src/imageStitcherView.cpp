#include "imageStitcherView.hpp"
// #include "core/qtCommon/cv2qt.hpp"

#include <omp.h>

#include <QBoxLayout>
#include <QButtonGroup>
#include <QDebug>
#include <QDir>
#include <QGridLayout>
#include <QImage>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextEdit>
#include <sstream>
#include <string>

#include "core/qtCommon/cv2qt.hpp"

namespace ImageStitch {

const bool kUseThread = true;

ImageStitcherView::ImageStitcherView(QWidget *parent) : QWidget(parent) {
  SetupUi(420, 290);
}
void ImageStitcherView::SetupUi(const int width, const int height) {
  input_image_box = new ImageBox();
  m_model = new ImageItemModel();
  input_image_box->setModel(m_model);
  stitch_button = new QPushButton();
  stitch_button->setText(tr("拼接"));
  stitch_button->setMaximumWidth(100);
  config_button = new QPushButton();
  config_button->setText(tr("配置"));
  config_button->setMaximumWidth(100);
  next_result = new QPushButton();
  next_result->setText(tr(">"));
  next_result->setMaximumWidth(40);
  next_result->setVisible(false);
  pre_result = new QPushButton();
  pre_result->setText(tr("<"));
  pre_result->setMaximumWidth(40);
  pre_result->setVisible(false);
  clean_images = new QPushButton();
  clean_images->setText(tr("清空"));
  clean_images->setMaximumWidth(100);
  clean_images->setEnabled(false);
  statusbar = new QStatusBar();
  result_view_widget = new ImageView();

  setStyleSheet("QStatusBar {border-top: 1px solid blue;}");

  QHBoxLayout *hboxLayout = new QHBoxLayout();
  hboxLayout->addWidget(pre_result, 0, Qt::AlignVCenter | Qt::AlignRight);
  hboxLayout->addWidget(next_result, 0, Qt::AlignVCenter | Qt::AlignLeft);
  hboxLayout->setSpacing(0);
  hboxLayout->setContentsMargins(10, 0, 10, 0);

  QVBoxLayout *gridLayout = new QVBoxLayout();
  QSplitter *splitter = new QSplitter(Qt::Horizontal);
  QVBoxLayout *vlayout1 = new QVBoxLayout();

  vlayout1->addWidget(input_image_box);
  vlayout1->addWidget(stitch_button, 0, Qt::AlignCenter);
  vlayout1->addWidget(config_button, 0, Qt::AlignCenter);
  vlayout1->addWidget(clean_images, 0, Qt::AlignCenter);
  vlayout1->addLayout(hboxLayout, 0);
  vlayout1->setSpacing(10);
  vlayout1->setContentsMargins(5, 0, 0, 0);
  QWidget *widget = new QWidget();
  widget->setLayout(vlayout1);
  splitter->addWidget(widget);
  splitter->addWidget(result_view_widget);

  gridLayout->addWidget(splitter, 1);
  gridLayout->addWidget(statusbar);
  gridLayout->setSpacing(10);
  gridLayout->setMargin(5);

  resize(width, height);
  setLayout(gridLayout);

  image_stitcher.signal_run_message.connect(
      &ImageStitcherView::_Solts::ShowMessage, &_solts);
  image_stitcher.signal_run_progress.connect(
      &ImageStitcherView::_Solts::SetProgress, &_solts);
  image_stitcher.signal_result.connect(&ImageStitcherView::_Solts::Result,
                                       &_solts);

  connect(stitch_button, &QPushButton::clicked, this,
          &ImageStitcherView::Stitch);
  connect(config_button, &QPushButton::clicked, [this](bool checker) {
    ConfigDialog *config_dialog = new ConfigDialog;
    CustomizeTitleWidget *config_window = new CustomizeTitleWidget();
    auto window_flags = config_window->windowFlags();
    config_window->setParent(this);
    config_window->setWindowFlags(window_flags);
    config_window->setWindowTitle(tr("配置"));
    config_window->setCentralWidget(config_dialog);
    // config_window->setWindowResizable(false);
    auto configs = image_stitcher.ParamTable();
    const auto &params = image_stitcher.GetParams();
    int i = 0;
    for (auto &config : configs) {
      if (config.type == ConfigItem::STRING) {
        QVector<QString> options;
        for (auto &option : config.options) {
          options.push_back(option.c_str());
        }
        config_dialog->AddItem(config.title.c_str(), options,
                               tr(config.description.c_str()));
        auto current = params.GetParam(config.title, std::string());
        config_dialog->SetDafultValue(i, current.c_str());
      } else if (config.type == ConfigItem::FLOAT) {
        QString prefix = "", suffix = "";
        if (config.options.size() == 1) {
          suffix = config.options[0].c_str();
        } else if (config.options.size() == 2) {
          prefix = config.options[0].c_str();
          suffix = config.options[1].c_str();
        }
        config_dialog->AddItem(config.title.c_str(), config.range[0],
                               config.range[1], config.description.c_str(),
                               prefix, suffix);
        auto current = params.GetParam(config.title,
                                       (config.range[0] + config.range[1]) / 2);
        config_dialog->SetDafultValue(i, current);
      } else if (config.type == ConfigItem::INT) {
        QString prefix = "", suffix = "";
        if (config.options.size() == 1) {
          suffix = config.options[0].c_str();
        } else if (config.options.size() == 2) {
          prefix = config.options[0].c_str();
          suffix = config.options[1].c_str();
        }
        config_dialog->AddItem(config.title.c_str(), (int)config.range[0],
                               (int)config.range[1], config.description.c_str(),
                               prefix, suffix);
        auto current = params.GetParam(
            config.title, (int)(config.range[0] + config.range[1]) / 2);
        config_dialog->SetDafultValue(i, current);
      }
      ++i;
    }
    connect(config_dialog, &ConfigDialog::accepted, this,
            &ImageStitcherView::ConfigurationStitcher);
    config_window->setAttribute(Qt::WA_DeleteOnClose, true);
    config_window->setWindowFlags(config_window->windowFlags() | Qt::Dialog);
    config_window->setWindowModality(Qt::ApplicationModal);
    config_window->show();
  });
  connect(next_result, &QPushButton::clicked, this,
          &ImageStitcherView::ShowNextResults);
  connect(pre_result, &QPushButton::clicked, this,
          &ImageStitcherView::ShowPreResults);
  connect(this, &ImageStitch::ImageStitcherView::Message, statusbar,
          &QStatusBar::showMessage);
  connect(clean_images, &QPushButton::clicked, this, [this](bool checked) {
    stitch_button->setText(tr("拼接"));
    stitch_button->disconnect();
		image_stitcher.Clean();
    connect(stitch_button, &QPushButton::clicked, this,
            &ImageStitcherView::Stitch);
    clean_images->setEnabled(false);
    _solts.current_index = -1;
    _solts.images.clear();
    pre_result->setVisible(false);
    next_result->setVisible(false);
    Message("", 1);
    ShowMessage(tr("欢迎使用图像拼接软件（NekoIS）\n") +
                tr("请拖拽或右键添加图片到待拼接图像列表中\n") +
                tr("点击拼接可以完成图像拼接\n") +
                tr("点击配置可以更改拼接算法"));
  });
  ShowMessage(tr("欢迎使用图像拼接软件（NekoIS）\n") +
              tr("请拖拽或右键添加图片到待拼接图像列表中\n") +
              tr("点击拼接可以完成图像拼接\n") +
              tr("点击配置可以更改拼接算法"));
}

void ImageStitcherView::ShowNextResults() {
  if (_solts.images.size() == 0) {
    return;
  }
  _solts.current_index = (_solts.current_index + 1) % _solts.images.size();
  ShowImage(_solts.images[_solts.current_index]);
}

void ImageStitcherView::ShowPreResults() {
  if (_solts.images.size() == 0) {
    return;
  }
  int size = _solts.images.size();
  _solts.current_index = (_solts.current_index - 1 + size) % size;
  ShowImage(_solts.images[_solts.current_index]);
}

void ImageStitcherView::CreateFromDirectory(const QString &path) {
  QDir dir(path);
  if (dir.exists()) {
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList{"*.png", "*.jpg", ".jpeg"});
    QFileInfoList list = dir.entryInfoList();
    std::vector<QPixmap> pixmap(list.size());
#pragma omp parallel for
    for (int i = 0; i < list.size(); ++i) {
      QFileInfo file_info = list.at(i);
      pixmap[i] = QPixmap(file_info.filePath());
    }
    for (int i = 0; i < list.size(); ++i) {
      m_model->addItem(pixmap[i], list[i].filePath());
    }
    Message(tr("文件夹: ") + path + tr(" .中的图片已成功导入"), 10000);
  } else {
    Message(tr("文件夹: ") + path + tr(" .不存在"), 10000);
  }
}

void ImageStitcherView::ShowImage(const QPixmap &pixmap) {
  result_view_widget->SetPixmap(pixmap);
}
void ImageStitcherView::ShowImage(const QImage &image) {
  result_view_widget->SetImage(image);
}

void ImageStitcherView::ShowMessage(const QString &message) {
  result_view_widget->SetPixmap(QPixmap());
  result_view_widget->ShowMessage(message);
}

void ImageStitcherView::_Solts::SetProgress(const float progress) {
  current_progress = progress;
  parent->StitchProgress(progress);
  if (progress >= 1) {
    parent->stitch_button->setEnabled(true);
  }
}
void ImageStitcherView::_Solts::ShowMessage(const std::string &message,
                                            int timeout) {
  current_message = message.c_str();
  parent->Message(current_message, timeout);
}
void ImageStitcherView::_Solts::Result(std::vector<ImagePtr> imgs) {
  parent->stitch_button->setEnabled(true);
  LOG(INFO) << "stitcher finished";
  LOG(INFO) << "result size : " << imgs.size();
  for (auto image : imgs) {
    images.push_back(cv2qt::CvMat2QImage(*image));
  }
  if (images.size() > 0) {
    parent->ShowImage(images[0]);
    current_index = 0;
  }
  parent->EndStitcher(imgs.size());
}

void ImageStitcherView::Stitch(bool checked) {
  StartStitcher();

  auto Items = m_model->getItems(0, m_model->rowCount());
  image_stitcher.RemoveAllImages();
  std::vector<std::string> image_files;
  for (const auto &[file_name, pixmap] : Items) {
    // ImagePtr img = ImagePtr(new
    // Image(cv2qt::QImage2CvMat(pixmap.toImage())));
    image_files.push_back(file_name.toStdString());
  }
  image_stitcher.SetImages(image_files);
  if (kUseThread) {
    task = std::async(std::launch::async,
                      [this]() { return image_stitcher.Stitch(); });
  } else {
    image_stitcher.Stitch();
  }
}

void ImageStitcherView::ShowMidData(bool checked) {
  CustomizeTitleWidget *main_window = new CustomizeTitleWidget();
  auto window_flags = main_window->windowFlags();
  main_window->setParent(this);
  main_window->setWindowFlags(window_flags);
  MidDataView *view = new MidDataView(&image_stitcher);
  main_window->setWindowTitle(tr("中间数据"));
  main_window->setCentralWidget(view);
  main_window->setAttribute(Qt::WA_DeleteOnClose);
  main_window->show();
}

void ImageStitcherView::ConfigurationStitcher(
    QVector<QPair<QString, QVariant>> settings) {
  Parameters &params = image_stitcher.GetParams();
  for (const auto &[item, option] : settings) {
    if (!option.isNull()) {
      if (option.type() == QVariant::String) {
        params.SetParam(item.toStdString(),
                        option.value<QString>().toStdString());
      } else if (option.type() == QVariant::Int) {
        params.SetParam(item.toStdString(), option.value<int>());
      } else if (option.type() == QVariant::Double) {
        params.SetParam(item.toStdString(), option.value<double>());
      }
    }
  }
  image_stitcher.SetParams();
}

void ImageStitcherView::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);
}

void ImageStitcherView::StartStitcher() {
  stitch_button->setEnabled(false);
  config_button->setEnabled(false);
}
void ImageStitcherView::EndStitcher(int result) {
  config_button->setEnabled(true);
  stitch_button->setText(tr("更多"));
  stitch_button->disconnect();
  connect(stitch_button, &QPushButton::clicked, this,
          &ImageStitcherView::ShowMidData);
  clean_images->setEnabled(true);
  if (result > 1) {
    next_result->setVisible(true);
    pre_result->setVisible(true);
  } else {
    next_result->setVisible(false);
    pre_result->setVisible(false);
  }
}

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
  resize(600, 800);
  setWindowModality(Qt::ApplicationModal);

  scroll_area = new QScrollArea();
  view = new QWidget();
  scroll_area->setWidget(view);
  form_layout = new QFormLayout();
  view->setLayout(form_layout);
  // view->setFixedWidth(570);

  // Create confirm and cancel buttons
  confirm_button = new QPushButton(tr("确认"));
  cancel_button = new QPushButton(tr("取消"));

  // Set button geometry
  QGridLayout *grid_layout = new QGridLayout();

  grid_layout->addWidget(scroll_area, 0, 0, 1, 5);
  grid_layout->addWidget(confirm_button, 1, 1);
  grid_layout->addWidget(cancel_button, 1, 3);
  grid_layout->setMargin(10);
  grid_layout->setSpacing(5);

  grid_layout->setRowStretch(1, 1);

  QVBoxLayout *main_layout = new QVBoxLayout();
  // main_layout->addWidget(title_bar);
  main_layout->addLayout(grid_layout, 1);
  main_layout->setMargin(0);
  main_layout->setSpacing(0);

  // confirm_button->setGeometry(QRect(120, 750, 80, 30));
  confirm_button->setStyleSheet(
      "QPushButton{background:#00BFFF}"
      "QPushButton::hover{background:rgb(110,115,100);}"
      "QPushButton::pressed{background:#eb7350;}"
      "QPushButton{border: 1px solid blue}"
      "QPushButton{border-radius: 3px}"
      "QPushButton{padding: 2px 10px 2px 10px}");
  // cancel_button->setGeometry(QRect(400, 750, 80, 30));
  setLayout(main_layout);
  // Connect button signals to slots
  connect(confirm_button, &QPushButton::clicked, this, &ConfigDialog::accept);
  connect(cancel_button, &QPushButton::clicked, this, &ConfigDialog::reject);
}

void ConfigDialog::SetDafultValue(const int index, const QString &text) {
  if (index >= 0 && index < config_box.size()) {
    auto item = dynamic_cast<QComboBox *>(config_box[index]);
    if (item == nullptr) {
      qDebug() << "SetDafultValue failed!!!";
      return;
    }
    item->setCurrentText(text);
  }
}

void ConfigDialog::SetDafultValue(const int index, const double value) {
  if (index >= 0 && index < config_box.size()) {
    auto item = dynamic_cast<QDoubleSpinBox *>(config_box[index]);
    if (item == nullptr) {
      qDebug() << "SetDafultValue failed!!!";
      return;
    }
    item->setValue(value);
  }
}

void ConfigDialog::SetDafultValue(const int index, const int value) {
  if (index >= 0 && index < config_box.size()) {
    auto item = dynamic_cast<QSpinBox *>(config_box[index]);
    if (item == nullptr) {
      qDebug() << "SetDafultValue failed!!!";
      return;
    }
    item->setValue(value);
  }
}

void ConfigDialog::accept() {
  QVector<QPair<QString, QVariant>> settings;
  for (int i = 0; i < form_layout->rowCount(); ++i) {
    auto labelItem = form_layout->itemAt(i, QFormLayout::LabelRole);
    auto fieldItem = form_layout->itemAt(i, QFormLayout::FieldRole);
    if (labelItem && fieldItem) {
      auto label = dynamic_cast<QLabel *>(labelItem->widget());
      if (dynamic_cast<QComboBox *>(fieldItem->widget()) != nullptr) {
        settings.push_back(
            {label->text(),
             dynamic_cast<QComboBox *>(fieldItem->widget())->currentText()});
      } else if (dynamic_cast<QDoubleSpinBox *>(fieldItem->widget()) !=
                 nullptr) {
        settings.push_back(
            {label->text(),
             dynamic_cast<QDoubleSpinBox *>(fieldItem->widget())->value()});
      } else if (dynamic_cast<QSpinBox *>(fieldItem->widget()) != nullptr) {
        settings.push_back(
            {label->text(),
             dynamic_cast<QSpinBox *>(fieldItem->widget())->value()});
      }
    }
  }
  accepted(settings);
  window()->close();
}
void ConfigDialog::reject() {
  rejected();
  QDialog::reject();
  window()->close();
}

void ConfigDialog::AddItem(const QString &title, const QVector<QString> &item,
                           const QString &description) {
  QComboBox *item_box = new QComboBox();
  item_box->setEditable(false);
  for (const auto &option : item) {
    item_box->addItem(option);
  }
  QLabel *label = new QLabel(title);
  if (!description.isEmpty()) {
    label->setToolTip(description);
  }
  form_layout->addRow(label, item_box);
  config_box.push_back(item_box);
  view->adjustSize();
}

void ConfigDialog::AddItem(const QString &title, const double &min_value,
                           const double &max_value, const QString &description,
                           const QString &prefix, const QString &suffix) {
  QDoubleSpinBox *item_box = new QDoubleSpinBox();
  item_box->setRange(min_value, max_value);  // 变化范围
  item_box->setDecimals(5);
  item_box->setSingleStep(1);
  item_box->setPrefix(prefix);
  item_box->setSuffix(suffix);

  QLabel *label = new QLabel(title);
  if (!description.isEmpty()) {
    label->setToolTip(description);
  }
  form_layout->addRow(label, item_box);
  config_box.push_back(item_box);
  view->adjustSize();
}

void ConfigDialog::AddItem(const QString &title, const int &min_value,
                           const int &max_value, const QString &description,
                           const QString &prefix, const QString &suffix) {
  QSpinBox *item_box = new QSpinBox();
  item_box->setRange(min_value, max_value);  // 变化范围
  item_box->setSingleStep(1);
  item_box->setPrefix(prefix);
  item_box->setSuffix(suffix);

  QLabel *label = new QLabel(title);
  if (!description.isEmpty()) {
    label->setToolTip(description);
  }
  form_layout->addRow(label, item_box);
  config_box.push_back(item_box);
  view->adjustSize();
}

MidDataView::MidDataView(ImageStitcher *image_stitcher, QWidget *parent)
    : QWidget(parent), image_stitcher(image_stitcher) {
  SetupUi();
}
void MidDataView::SetupUi() {
  grid_layout = new QGridLayout();
  setLayout(grid_layout);
  int image_length = image_stitcher->FinalStitchImages().size();

  features_buttons.resize(image_length);
  original_buttons.resize(image_length);
  matches_buttons.resize(image_length,
                         std::vector<QPushButton *>(image_length));
  compensator_buttons.resize(image_length);
  seam_mask_buttons.resize(image_length);

  label = new QLabel();
  label->setText("特征图\\原图");

  grid_layout->addWidget(label, 0, 0);

  for (int i = 0; i < image_length; ++i) {
    original_buttons[i] = new QPushButton();
    connect(
        original_buttons[i], &QPushButton::clicked, this,
        [i, this](bool checked) {
          CustomizeTitleWidget *main_window = new CustomizeTitleWidget();
          auto window_flags = main_window->windowFlags();
          main_window->setParent(this);
          main_window->setWindowFlags(window_flags);
          ImageView *image_view = new ImageView();
          image_view->SetImage(GetImage(i));
          main_window->setCentralWidget(image_view);
          main_window->setWindowTitle(tr("原图 - ") + QString::number(i + 1));
          main_window->setAttribute(Qt::WA_DeleteOnClose, true);
          main_window->show();
        });
    original_buttons[i]->setText("o_" + QString::number(i + 1));
    grid_layout->addWidget(original_buttons[i], 0, i + 1);
  }
  for (int i = 0; i < image_length; ++i) {
    features_buttons[i] = new QPushButton();
    connect(features_buttons[i], &QPushButton::clicked, this,
            [i, this](bool checked) {
              CustomizeTitleWidget *main_window = new CustomizeTitleWidget();
              auto window_flags = main_window->windowFlags();
              main_window->setParent(this);
              main_window->setWindowFlags(window_flags);
              ImageView *image_view = new ImageView();
              QTextEdit *text_edit = new QTextEdit();
              QTextEdit *text_edit1 = new QTextEdit();
              QSplitter *splitter = new QSplitter();
              QSplitter *splitter1 = new QSplitter();
              splitter->addWidget(image_view);
              splitter1->setOrientation(Qt::Vertical);
              splitter1->addWidget(text_edit1);
              splitter1->addWidget(text_edit);
              splitter->addWidget(splitter1);

              text_edit1->setText(GetCameraParamsText(i));
              text_edit->setText(GetFeaturesText(i));
              text_edit->setReadOnly(true);
              image_view->SetImage(GetFeaturesImage(i));

              main_window->setCentralWidget(splitter);
              main_window->setWindowTitle(tr("特征提取图 - ") +
                                          QString::number(i + 1));
              main_window->setAttribute(Qt::WA_DeleteOnClose, true);
              main_window->show();
            });
    features_buttons[i]->setText("f_" + QString::number(i + 1));
    grid_layout->addWidget(features_buttons[i], i + 1, 0);
  }
  auto thresh =
      image_stitcher->GetParams().GetParam("PanoConfidenceThresh", 0.0);
  for (int i = 0; i < image_length; ++i) {
    for (int j = 0; j < image_length; ++j) {
      matches_buttons[i][j] = new QPushButton();
      connect(matches_buttons[i][j], &QPushButton::clicked, this,
              [i, j, this](bool checked) {
                CustomizeTitleWidget *main_window = new CustomizeTitleWidget();
                auto window_flags = main_window->windowFlags();
                main_window->setParent(this);
                main_window->setWindowFlags(window_flags);
                ImageView *image_view = new ImageView();
                ImageView *image_view1 = new ImageView();
                QTextEdit *text_edit = new QTextEdit();
                QSplitter *splitter = new QSplitter();
                QSplitter *splitter1 = new QSplitter();
                splitter1->setOrientation(Qt::Vertical);
                splitter1->addWidget(image_view);
                splitter1->addWidget(image_view1);
                splitter->addWidget(splitter1);
                splitter->addWidget(text_edit);

                text_edit->setText(GetMatchesText(i, j));
                text_edit->setReadOnly(true);
                image_view->SetImage(GetMatchesImage(i, j));
                image_view1->SetImage(GetWarpImage(i, j));

                main_window->setCentralWidget(splitter);

                main_window->setWindowTitle(tr("匹配图 - ") +
                                            QString::number(i + 1) + ", " +
                                            QString::number(j + 1));
                main_window->setAttribute(Qt::WA_DeleteOnClose, true);
                main_window->show();
              });
      auto matches = image_stitcher->FeaturesMatches()[{i, j}];
      matches_buttons[i][j]->setText(
          QString().setNum(matches.confidence, 'f', 6));
      if (matches.confidence >= thresh) {
        matches_buttons[i][j]->setAutoFillBackground(true);
        QPalette pal = matches_buttons[i][j]->palette();
        pal.setColor(QPalette::ButtonText, QColor(Qt::red));
        matches_buttons[i][j]->setPalette(pal);
      }
      if (i == j) {
        matches_buttons[i][j]->setEnabled(false);
      }
      grid_layout->addWidget(matches_buttons[i][j], i + 1, j + 1);
    }
  }

  auto compensator_label = new QLabel();
  compensator_label->setText("compensator result");
  grid_layout->addWidget(compensator_label, image_length + 1, 0);
  compensator_buttons.resize(image_length);
  for (int i = 0; i < image_length; ++i) {
    compensator_buttons[i] = new QPushButton();
    connect(
        compensator_buttons[i], &QPushButton::clicked, this,
        [i, this](bool checked) {
          CustomizeTitleWidget *main_window = new CustomizeTitleWidget();
          auto window_flags = main_window->windowFlags();
          main_window->setParent(this);
          main_window->setWindowFlags(window_flags);
          ImageView *image_view = new ImageView();
          ImageView *image_view1 = new ImageView();
          QSplitter *splitter = new QSplitter();

          splitter->addWidget(image_view);
          splitter->addWidget(image_view1);

          image_view->SetImage(GetCompensatorImage(i)[0]);
          image_view1->SetImage(GetCompensatorImage(i)[1]);

          main_window->setCentralWidget(splitter);

          main_window->setWindowTitle(tr("补偿 - ") + QString::number(i + 1));
          main_window->setAttribute(Qt::WA_DeleteOnClose, true);
          main_window->show();
        });
    compensator_buttons[i]->setText(QString::number(i + 1));
    grid_layout->addWidget(compensator_buttons[i], image_length + 1, i + 1);
  }

  auto seam_label = new QLabel();
  seam_label->setText("seam find result");
  grid_layout->addWidget(seam_label, image_length * 2 + 1, 0);
  seam_mask_buttons.resize(image_length);
  for (int i = 0; i < image_length; ++i) {
    seam_mask_buttons[i] = new QPushButton();
    connect(seam_mask_buttons[i], &QPushButton::clicked, this,
            [i, this](bool checked) {
              CustomizeTitleWidget *main_window = new CustomizeTitleWidget();
              auto window_flags = main_window->windowFlags();
              main_window->setParent(this);
              main_window->setWindowFlags(window_flags);
              ImageView *image_view = new ImageView();

              image_view->SetImage(GetSeamMaskImage(i));

              main_window->setCentralWidget(image_view);

              main_window->setWindowTitle(tr("拼接缝裁剪 - ") +
                                          QString::number(i + 1));
              main_window->setAttribute(Qt::WA_DeleteOnClose, true);
              main_window->show();
            });
    seam_mask_buttons[i]->setText(QString::number(i + 1));
    grid_layout->addWidget(seam_mask_buttons[i], image_length * 2 + 1, i + 1);
  }
}

QImage MidDataView::GetImage(const int index) {
  if (images.size() != image_stitcher->FinalStitchImages().size()) {
    images.resize(image_stitcher->FinalStitchImages().size());
  }
  if (index < 0 || index >= images.size()) {
    return QImage();
  }
  if (images[index].isNull()) {
    images[index] =
        cv2qt::CvMat2QImage(image_stitcher->FinalStitchImages()[index]);
  }
  return images[index];
}
QImage MidDataView::GetFeaturesImage(const int index) {
  if (index >= 0 && index < image_stitcher->ImagesFeatures().size()) {
    if (features_images.size() != image_stitcher->ImagesFeatures().size()) {
      features_images.resize(image_stitcher->ImagesFeatures().size());
    }
    if (features_images[index].isNull()) {
      features_images[index] = cv2qt::CvMat2QImage(image_stitcher->DrawKeypoint(
          image_stitcher->FinalStitchImages()[index],
          image_stitcher->ImagesFeatures()[index]));
    }
    return features_images[index];
  }
  return QImage();
}
QString MidDataView::GetFeaturesText(const int index) {
  if (index < 0 || index >= image_stitcher->ImagesFeatures().size()) {
    return QString();
  }
  std::ostringstream text;
  auto features = image_stitcher->ImagesFeatures()[index];
  text << "img_idx : " << features.img_idx << std::endl;
  text << "img_size : " << features.img_size << std::endl;
  text << "keypoint size : " << features.keypoints.size() << std::endl;
  text << "keypoints : " << std::endl;
  for (int i = 0; i < features.keypoints.size(); ++i) {
    text << features.keypoints[i].pt << std::endl;
  }
  text << "description : " << std::endl;
  text << features.descriptors << std::endl;
  return QString(text.str().c_str());
}
QString MidDataView::GetMatchesText(const int index1, const int index2) {
  std::ostringstream text;
  auto matches = image_stitcher->FeaturesMatches()[{index1, index2}];
  text << "src_idx : " << matches.src_img_idx << std::endl;
  text << "dst_idx : " << matches.dst_img_idx << std::endl;
  text << "confidence : " << matches.confidence << std::endl;
  text << "H : " << std::endl;
  text << matches.H << std::endl;
  text << "inlier point size : " << matches.num_inliers << std::endl;
  for (int i = 0; i < matches.getInliers().size(); ++i) {
    text << int(matches.getInliers()[i]) << " ";
  }
  text << std::endl;
  text << "DMatches : " << std::endl;
  for (int i = 0; i < matches.getMatches().size(); ++i) {
    const auto &dmatch = matches.getMatches()[i];
    text << dmatch.imgIdx << " " << dmatch.queryIdx << " " << dmatch.trainIdx
         << " " << dmatch.distance << std::endl;
  }
  return QString(text.str().c_str());
}
QString MidDataView::GetCameraParamsText(const int index) {
  if (index < 0 || index >= image_stitcher->FinalStitchImages().size()) {
    return QString();
  }
  std::ostringstream text;
  auto cameraParams = GetCameraParams(index);
  text << "camera params : " << std::endl;
  text << "focal : " << cameraParams.focal << std::endl;
  text << "aspect : " << cameraParams.aspect << std::endl;
  text << "ppx : " << cameraParams.ppx << std::endl;
  text << "ppy : " << cameraParams.ppy << std::endl;
  text << "R : \n" << cameraParams.R << std::endl;
  text << "t : \n" << cameraParams.t << std::endl;
  text << "K : \n" << cameraParams.K() << std::endl;
  return QString(text.str().c_str());
}
CameraParams MidDataView::GetCameraParams(const int index) {
  if (index < 0 || index >= image_stitcher->FinalStitchImages().size()) {
    return CameraParams();
  }
  if (camera_params.size() != image_stitcher->FinalStitchImages().size()) {
    camera_params.resize(image_stitcher->FinalStitchImages().size());
    auto comp = image_stitcher->component();
    for (int i = 0; i < comp.size(); ++i) {
      camera_params[comp[i]] = image_stitcher->FinalCameraParams()[i];
    }
  }
  return camera_params[index];
}
QImage MidDataView::GetMatchesImage(const int index1, const int index2) {
  int length = image_stitcher->ImagesFeatures().size();
  if (index1 < 0 || index1 >= length) {
    return QImage();
  }
  if (index2 < 0 || index2 >= length) {
    return QImage();
  }
  if (matches_images.size() != length) {
    matches_images.resize(length, std::vector<QImage>(length));
  }
  if (matches_images[index1][index2].isNull()) {
    const auto &image1 = image_stitcher->FinalStitchImages()[index1];
    const auto &image2 = image_stitcher->FinalStitchImages()[index2];
    const auto &features1 = image_stitcher->ImagesFeatures()[index1];
    const auto &features2 = image_stitcher->ImagesFeatures()[index2];
    auto &matches = image_stitcher->FeaturesMatches()[{index1, index2}];
    matches_images[index1][index2] =
        cv2qt::CvMat2QImage(image_stitcher->DrawMatches(
            image1, features1, image2, features2, matches));
  }
  return matches_images[index1][index2];
}
QImage MidDataView::GetSeamMaskImage(const int index) {
  int length = image_stitcher->FinalStitchImages().size();
  if (index < 0 || index >= length) {
    return QImage();
  }
  if (seam_mask_images.size() != length) {
    seam_mask_images.resize(length);
  }
  if (seam_mask_images[index].isNull()) {
    auto &comp = image_stitcher->component();
    int i = std::lower_bound(comp.begin(), comp.end(), index) - comp.begin();
    if (i < comp.size() && comp[i] == index) {
      seam_mask_images[index] =
          cv2qt::CvMat2QImage(image_stitcher->SeamMasks()[i]);
    } else {
      return QImage();
    }
  }
  return seam_mask_images[index];
}

std::vector<QImage> MidDataView::GetCompensatorImage(const int index) {
  int length = image_stitcher->FinalStitchImages().size();
  if (index < 0 || index >= length) {
    return std::vector<QImage>(2);
  }
  if (compensator_images.size() != length) {
    compensator_images.resize(length);
  }
  if (compensator_images[index].empty()) {
    auto &comp = image_stitcher->component();
    int i = std::lower_bound(comp.begin(), comp.end(), index) - comp.begin();
    if (i < comp.size() && comp[i] == index) {
      for (const auto &image : image_stitcher->CompensatorImages()[i]) {
        compensator_images[index].push_back(cv2qt::CvMat2QImage(image));
      }
    } else {
      return std::vector<QImage>(2);
    }
  }
  return compensator_images[index];
}

QImage MidDataView::GetWarpImage(const int index1, const int index2) {
  int length = image_stitcher->ImagesFeatures().size();
  if (index1 < 0 || index1 >= length) {
    return QImage();
  }
  if (index2 < 0 || index2 >= length) {
    return QImage();
  }
  return cv2qt::CvMat2QImage(
      image_stitcher->WarperImageByCameraParams(index1, index2));
}
}  // namespace ImageStitch