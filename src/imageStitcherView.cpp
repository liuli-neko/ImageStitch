#include "imageStitcherView.hpp"
// #include "core/qtCommon/cv2qt.hpp"

#include <QDebug>
#include <QDir>
#include <QGridLayout>
#include <QSpinBox>
#include <QStatusBar>

#include "core/qtCommon/cv2qt.hpp"

namespace ImageStitch {
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
  hboxLayout->addWidget(pre_result, 0, Qt::AlignCenter);
  hboxLayout->addWidget(next_result, 0, Qt::AlignCenter);
  hboxLayout->setSpacing(10);
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

  connect(stitch_button, &QPushButton::clicked, [this](bool checked) {
    stitch_button->setEnabled(false);
    Stitch();
  });
  connect(config_button, &QPushButton::clicked, [this](bool checker) {
    ConfigDialog *config_dialog = new ConfigDialog;
    CustomizeTitleWidget *config_window = new CustomizeTitleWidget();
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
    clean_images->setEnabled(false);
    _solts.current_index = -1;
    _solts.images.clear();
    pre_result->setVisible(false);
    next_result->setVisible(false);
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
    for (int i = 0; i < list.size(); ++i) {
      QFileInfo file_info = list.at(i);
      std::cout << file_info.filePath().toStdString() << std::endl;
      m_model->addItem(file_info.filePath());
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
  int n = images.size();
  for (auto image : imgs) {
    images.push_back(cv2qt::CvMat2QImage(*image));
  }
  if (images.size() > 0) {
    parent->ShowImage(images[n]);
    parent->clean_images->setEnabled(true);
    current_index = n;
  }
  if (images.size() > 1) {
    parent->next_result->setVisible(true);
    parent->pre_result->setVisible(true);
  } else {
    parent->next_result->setVisible(false);
    parent->pre_result->setVisible(false);
  }
}

void ImageStitcherView::Stitch() {
  auto Items = m_model->getItems(0, m_model->rowCount());
  image_stitcher.RemoveAllImages();
  std::vector<std::string> image_files;
  for (const auto &[file_name, pixmap] : Items) {
    // ImagePtr img = ImagePtr(new
    // Image(cv2qt::QImage2CvMat(pixmap.toImage())));
    image_files.push_back(file_name.toStdString());
  }
  image_stitcher.SetImages(image_files);
  task = std::async(std::launch::async,
                    [this]() { return image_stitcher.Stitch(); });
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
  QPainter painter(this);
  painter.setPen(QColor("blue"));
  auto image_rect = result_view_widget->geometry();
  painter.drawRect(image_rect.x() - 1, image_rect.y() - 1,
                   image_rect.width() + 1, image_rect.height() + 1);
  QWidget::paintEvent(event);
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

}  // namespace ImageStitch