#include "imageStitcherView.hpp"
// #include "core/qtCommon/cv2qt.hpp"

#include <QDir>
#include <QGridLayout>

#include "core/qtCommon/cv2qt.hpp"

namespace ImageStitch {
ImageStitcherView::ImageStitcherView(QWidget *parent) : QWidget(parent) {
  SetupUi(420, 290);
}
void ImageStitcherView::SetupUi(const int width, const int height) {
  input_image_box = new ImageBox();
  m_model = new ImageItemModel();
  input_image_box->setModel(m_model);
  input_image_box->setMaximumWidth(300);
  result_image_view = new ImageView();
  stitch_button = new QPushButton();
  stitch_button->setText("拼接");
  stitch_button->setMaximumWidth(100);
  config_button = new QPushButton();
  config_button->setText("配置");
  config_button->setMaximumWidth(100);

  QGridLayout *gridLayout = new QGridLayout();

  gridLayout->addWidget(input_image_box, 0, 0);
  gridLayout->addWidget(stitch_button, 1, 0, 1, 1, Qt::AlignCenter);
  gridLayout->addWidget(config_button, 2, 0, 1, 1, Qt::AlignCenter);
  gridLayout->addWidget(result_image_view, 0, 1, 3, 1);
  gridLayout->setSpacing(10);
  gridLayout->setMargin(5);
  gridLayout->setColumnStretch(0, 1);
  gridLayout->setColumnStretch(1, 5);

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
    config_window->setWindowTitle("Stitcher Configuration");
    config_window->setCentralWidget(config_dialog);
    config_window->setWindowResizable(false);
    auto configs = image_stitcher.ParamTable();
    const auto &params = image_stitcher.GetParams();
    int i = 0;
    for (auto &config : configs) {
      QVector<QString> options;
      options.push_back("default");
      for (auto &option : config.options) {
        options.push_back(option.c_str());
      }
      config_dialog->AddItem(config.title.c_str(), options,
                             config.description.c_str());
      auto current = params.GetParam(config.title, std::string("default"));
      config_dialog->SetDafultText(i, current.c_str());
      ++i;
    }
    connect(config_dialog, &ConfigDialog::accepted, this,
            &ImageStitcherView::ConfigurationStitcher);
    config_window->setAttribute(Qt::WA_DeleteOnClose, true);
    config_window->setWindowFlags(config_window->windowFlags() | Qt::Dialog);
    config_window->setWindowModality(Qt::ApplicationModal);
    config_window->show();
  });
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
    Message("文件夹: " + path + " .中的图片已成功导入", 10000);
  } else {
    Message("文件夹: " + path + " .不存在", 10000);
  }
  // TODO: 导出当前stitcher设置
  // TODO: 从文件导入stitcher设置
}

void ImageStitcherView::ShowImage(const QPixmap &pixmap) {
  result_image_view->SetPixmap(pixmap);
}
void ImageStitcherView::ShowImage(const QImage &image) {
  result_image_view->SetImage(image);
}

void ImageStitcherView::ShowMessage(const QString &message) {
  result_image_view->ShowMessage(message);
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
void ImageStitcherView::_Solts::Result(ImagePtr img) {
  current_result = img;
  parent->stitch_button->setEnabled(true);
  parent->ShowImage(cv2qt::CvMat2QImage(*img));
}

void ImageStitcherView::Stitch() {
  auto Items = m_model->getItems(0, m_model->rowCount());
  image_stitcher.RemoveAllImages();
  for (const auto &[file_name, pixmap] : Items) {
    ImagePtr img = ImagePtr(new Image(cv2qt::QImage2CvMat(pixmap.toImage())));
    image_stitcher.AddImage(img);
  }
  task = std::async(std::launch::async, [this]() { image_stitcher.Stitch(); });
}

void ImageStitcherView::ConfigurationStitcher(
    QVector<QPair<QString, QString>> settings) {
  Parameters params;
  for (const auto &[item, option] : settings) {
    params.SetParam(item.toStdString(), option.toStdString());
  }
  image_stitcher.SetParams(params);
}

void ImageStitcherView::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setPen(QColor("blue"));
  auto image_rect = result_image_view->geometry();
  painter.drawRect(image_rect.x() - 1, image_rect.y() - 1,
                   image_rect.width() + 1, image_rect.height() + 1);
  QWidget::paintEvent(event);
}

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
  setFixedSize(600, 800);
  setWindowModality(Qt::ApplicationModal);
  // setWindowFlags(Qt::WindowMinMaxButtonsHint | Qt::FramelessWindowHint);

  // title_bar = new TitleBar(this);
  // title_bar->setResizable(false);
  // title_bar->setStyleSheet("border: 1px solid blue");

  scroll_area = new QScrollArea();
  view = new QWidget();
  scroll_area->setWidget(view);
  form_layout = new QFormLayout();
  view->setLayout(form_layout);
  view->setFixedWidth(570);

  // Create confirm and cancel buttons
  confirm_button = new QPushButton("确认");
  cancel_button = new QPushButton("取消");

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
  confirm_button->setStyleSheet("background-color: lightblue;");
  // cancel_button->setGeometry(QRect(400, 750, 80, 30));
  setLayout(main_layout);
  // Connect button signals to slots
  connect(confirm_button, &QPushButton::clicked, this, &ConfigDialog::accept);
  connect(cancel_button, &QPushButton::clicked, this, &ConfigDialog::reject);
}

void ConfigDialog::SetDafultText(const int index, const QString &text) {
  if (index >= 0 && index < config_box.size()) {
    config_box[index]->setCurrentText(text);
  }
}

void ConfigDialog::SetDafultIndex(const int index, const int &option_index) {
  if (index >= 0 && index < config_box.size()) {
    config_box[index]->setCurrentIndex(option_index);
  }
}

void ConfigDialog::accept() {
  QVector<QPair<QString, QString>> settings;
  for (int i = 0; i < form_layout->rowCount(); ++i) {
    auto labelItem = form_layout->itemAt(i, QFormLayout::LabelRole);
    auto fieldItem = form_layout->itemAt(i, QFormLayout::FieldRole);
    if (labelItem && fieldItem) {
      auto label = dynamic_cast<QLabel *>(labelItem->widget());
      auto field = dynamic_cast<QComboBox *>(fieldItem->widget());
      settings.push_back({label->text(), field->currentText()});
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

}  // namespace ImageStitch