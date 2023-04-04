#include "mainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QGraphicsView>
#include <QGridLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

namespace Ui {
class MainWindow {
 public:
  QMenu *file_menu;
  QMenuBar *menuBar;
  QAction *action_add_tab;
  QAction *action_add_tab_from_files;
  QAction *action_export_current_tab;
  QAction *action_import_for_current_tab;
  // QMenuBar *menubar;
  QTabWidget *tab_widget;
  // QMenu *menuoriginal;
  QStatusBar *statusbar;

  void setupUi(::MainWindow *main_window) {
    if (main_window->objectName().isEmpty())
      main_window->setObjectName("Image Stitch(NekoIS)");
    main_window->setWindowTitle("Image Stitch(NekoIS)");
    main_window->setWindowIcon(
        QPixmap(
            "E:/workplace/qt_project/ImageStitch/src/imageStitcher-icon.png")
            .scaled(30, 30));
    main_window->resize(800, 600);

    tab_widget = new QTabWidget();
    main_window->setCentralWidget(tab_widget);

    auto image_stitch_central = new ImageStitch::ImageStitcherView();
    tab_widget->setMovable(true);
    tab_widget->setContextMenuPolicy(Qt::CustomContextMenu);
    tab_widget->installEventFilter(main_window);
    tab_widget->setAcceptDrops(true);

    QWidget::connect(tab_widget, &QTabWidget::customContextMenuRequested,
                     main_window, &::MainWindow::TabMenu);
    QWidget::connect(tab_widget, &QTabWidget::tabBarDoubleClicked, main_window,
                     &::MainWindow::TabBarEditor);
    QWidget::connect(tab_widget, &QTabWidget::tabCloseRequested, main_window,
                     &::MainWindow::TabClose);
    image_stitch_central->setSizePolicy(QSizePolicy::Expanding,
                                        QSizePolicy::Expanding);

    action_add_tab = new QAction();
    action_add_tab->setObjectName("action_add_tab");
    QWidget::connect(action_add_tab, &QAction::triggered, main_window,
                     [main_window, this](bool checked) -> void {
                       auto tab = main_window->addTab();
                       tab_widget->setCurrentWidget(tab);
                     });
    action_add_tab_from_files = new QAction();
    action_add_tab_from_files->setObjectName("action_add_tab_from_files");
    QWidget::connect(action_add_tab_from_files, &QAction::triggered,
                     main_window, [main_window, this](bool checked) -> void {
                       auto tab = main_window->addTab();
                       tab_widget->setCurrentWidget(tab);
                       QString dir_name = QFileDialog::getExistingDirectory(
                           main_window, main_window->tr("添加图像"), "./");
                       tab->CreateFromDirectory(dir_name);
                     });
    action_export_current_tab = new QAction();
    action_export_current_tab->setObjectName("action_export_current_tab");
    QWidget::connect(
        action_export_current_tab, &QAction::triggered, main_window,
        [main_window, this](bool checked) {
          auto tab = dynamic_cast<ImageStitch::ImageStitcherView *>(
              tab_widget->currentWidget());
          auto params = tab->Params();
          QString tab_title = tab_widget->tabText(tab_widget->currentIndex());
          QString file_name = QFileDialog::getSaveFileName(
              main_window, main_window->tr("导出配置文件"),
              "./" + tab_title + ".json", "configuration (*.json)");
          QFile file(file_name);
          file.open(QIODevice::WriteOnly);
          file.write(params.ToString().c_str());
          file.close();
        });
    action_import_for_current_tab = new QAction();
    action_import_for_current_tab->setObjectName(
        "action_import_for_current_tab");
    QWidget::connect(action_import_for_current_tab, &QAction::triggered,
                     main_window, [main_window, this](bool checked) {
                       auto tab =
                           dynamic_cast<ImageStitch::ImageStitcherView *>(
                               tab_widget->currentWidget());
                       QString file_name = QFileDialog::getOpenFileName(
                           main_window, main_window->tr("导入配置文件"), "./",
                           "configuration (*.json)");
                       QFile file(file_name);
                       file.open(QIODevice::ReadOnly);
                       auto data = file.readAll();
                       file.close();
                       Parameters params;
                       params.FromString(data);
                       tab->SetParams(params);
                     });

    menuBar = new QMenuBar();
    menuBar->setObjectName("menuBar");
    main_window->addMenuBar(menuBar);

    file_menu = new QMenu(menuBar);
    file_menu->setObjectName("file_menu");

    statusbar = new QStatusBar(main_window);
    statusbar->setObjectName("statusbar");
    main_window->setStatusBar(statusbar);

    menuBar->addAction(file_menu->menuAction());
    file_menu->addAction(action_add_tab);
    file_menu->addAction(action_add_tab_from_files);
    file_menu->addAction(action_export_current_tab);
    file_menu->addAction(action_import_for_current_tab);

    retranslateUi(main_window);

    QMetaObject::connectSlotsByName(main_window);
  }  // setupUi

  void retranslateUi(QWidget *MainWindow) {
    action_add_tab->setText("新建标签");
    action_add_tab_from_files->setText("从文件夹新建标签");
    action_export_current_tab->setText("导出当前标签配置");
    action_import_for_current_tab->setText("为当前标签导入配置");
    file_menu->setTitle("文件");
  }  // retranslateUi
};
}  // namespace Ui

MainWindow::MainWindow(QWidget *parent)
    : CustomizeTitleWidget(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  addTab();
}

void MainWindow::TabBarEditor(const int index) {
  if (index == -1) {
    return;
  }
  QLineEdit *lineEdit = new QLineEdit(ui->tab_widget);
  // 设置lineEdit的位置和大小为当前标签的矩形区域
  lineEdit->setGeometry(ui->tab_widget->tabBar()->tabRect(index));
  // 设置lineEdit的文本为当前标签的文本
  lineEdit->setText(ui->tab_widget->tabText(index));
  // 显示lineEdit并设置焦点
  lineEdit->show();
  lineEdit->setFocus();
  // 连接lineEdit的editingFinished信号到一个槽函数
  connect(lineEdit, &QLineEdit::editingFinished, [this, index, lineEdit]() {
    // 获取lineEdit的文本，并用它来更新当前标签的文本
    QString text = lineEdit->text();
    ui->tab_widget->setTabText(index, text);
    // 删除lineEdit对象，并恢复当前标签的显示
    lineEdit->deleteLater();
  });
}

void MainWindow::TabClose(const int index) {
  if (ui->tab_widget->count() == 1) {
    close();
  } else {
    ui->tab_widget->removeTab(index);
  }
}

void MainWindow::TabMenu(const QPoint &pos) {
  int index = ui->tab_widget->tabBar()->tabAt(pos);
  QMenu menu;
  if (index != -1) {
    menu.addAction(
        "关闭", [this, index]() { ui->tab_widget->tabCloseRequested(index); });
    menu.addAction("重命名", [this, index]() { TabBarEditor(index); });
  }
  menu.exec(ui->tab_widget->mapToGlobal(pos));
}

ImageStitch::ImageStitcherView *MainWindow::addTab(QString tabName) {
  ImageStitch::ImageStitcherView *imageStitchView =
      new ImageStitch::ImageStitcherView();
  ui->tab_widget->addTab(imageStitchView, "未命名");
  connect(imageStitchView, &ImageStitch::ImageStitcherView::Message,
          ui->statusbar, &QStatusBar::showMessage);
  imageStitchView->ShowMessage(
      "欢迎使用图像拼接软件（NekoIS）\n"
      "请拖拽或右键添加图片到待拼接图像列表中\n"
      "点击拼接可以完成图像拼接\n"
      "点击配置可以更改拼接算法");
  return imageStitchView;
}

void MainWindow::handleTableDragEnter(QDragEnterEvent *mouseEvent) {
  int index = ui->tab_widget->tabBar()->tabAt(mouseEvent->pos());
  // 如果索引有效
  if (index != -1) {
    ui->tab_widget->setCurrentIndex(index);
  }
}
bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (obj == ui->tab_widget && event->type() == QEvent::DragEnter) {
    handleTableDragEnter(static_cast<QDragEnterEvent *>(event));
  }
  return QObject::eventFilter(obj, event);
}

MainWindow::~MainWindow() { delete ui; }