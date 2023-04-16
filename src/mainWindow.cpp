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

  void setupUi(::MainWindow *main_window) {
    if (main_window->objectName().isEmpty())
      main_window->setObjectName("Image Stitch(NekoIS)");
    main_window->setWindowTitle("Image Stitch(NekoIS)");
    main_window->setWindowIcon(QPixmap("./ImageStitch.png").scaled(25, 25));
    main_window->resize(800, 600);

    tab_widget = new QTabWidget();
    main_window->setCentralWidget(tab_widget);

    auto image_stitch_central = new ImageStitch::ImageStitcherView();
    tab_widget->setMovable(true);
    tab_widget->setContextMenuPolicy(Qt::CustomContextMenu);
    tab_widget->installEventFilter(main_window);
    tab_widget->setAcceptDrops(true);

    image_stitch_central->setSizePolicy(QSizePolicy::Expanding,
                                        QSizePolicy::Expanding);

    action_add_tab = new QAction();
    action_add_tab->setObjectName("action_add_tab");

    action_add_tab_from_files = new QAction();
    action_add_tab_from_files->setObjectName("action_add_tab_from_files");

    action_export_current_tab = new QAction();
    action_export_current_tab->setObjectName("action_export_current_tab");

    action_import_for_current_tab = new QAction();
    action_import_for_current_tab->setObjectName(
        "action_import_for_current_tab");

    menuBar = new QMenuBar();
    menuBar->setObjectName("menuBar");
    main_window->addMenuBar(menuBar);

    file_menu = new QMenu(menuBar);
    file_menu->setObjectName("file_menu");

    menuBar->addAction(file_menu->menuAction());
    file_menu->addAction(action_add_tab);
    file_menu->addAction(action_add_tab_from_files);
    file_menu->addSeparator();
    file_menu->addAction(action_export_current_tab);
    file_menu->addAction(action_import_for_current_tab);

    retranslateUi(main_window);

    QMetaObject::connectSlotsByName(main_window);
  }  // setupUi

  void retranslateUi(QWidget *MainWindow) {
    action_add_tab->setText(MainWindow->tr("新建"));
    action_add_tab_from_files->setText(MainWindow->tr("打开文件夹"));
    action_export_current_tab->setText(MainWindow->tr("导出"));
    action_import_for_current_tab->setText(MainWindow->tr("导入"));
    file_menu->setTitle(MainWindow->tr("文件"));
  }  // retranslateUi
};
}  // namespace Ui

MainWindow::MainWindow(QWidget *parent)
    : CustomizeTitleWidget(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  connect(ui->tab_widget, &QTabWidget::customContextMenuRequested, this,
          &::MainWindow::TabMenu);
  connect(ui->tab_widget, &QTabWidget::tabBarDoubleClicked, this,
          &::MainWindow::TabBarEditor);
  connect(ui->tab_widget, &QTabWidget::tabCloseRequested, this,
          &::MainWindow::TabClose);

  connect(ui->action_add_tab, &QAction::triggered, this,
          [this](bool checked) -> void {
            auto tab = addTab();
            ui->tab_widget->setCurrentWidget(tab);
          });
  connect(ui->action_add_tab_from_files, &QAction::triggered, this,
          [this](bool checked) -> void {
            QFileDialog file_dialog;
            file_dialog.setWindowTitle(tr("添加图像"));
            file_dialog.setFileMode(QFileDialog::DirectoryOnly);
            file_dialog.setViewMode(QFileDialog::Detail);
            if (file_dialog.exec()) {
              QString dir_name = file_dialog.selectedFiles()[0];
              auto tab = addTab(QFileInfo(dir_name).baseName());
              ui->tab_widget->setCurrentWidget(tab);
              tab->CreateFromDirectory(dir_name);
            }
          });
  connect(ui->action_export_current_tab, &QAction::triggered, this,
          [this](bool checked) {
            auto tab = dynamic_cast<ImageStitch::ImageStitcherView *>(
                ui->tab_widget->currentWidget());
            auto params = tab->Params();
            QString tab_title =
                ui->tab_widget->tabText(ui->tab_widget->currentIndex());
            QFileDialog file_dialog;
            file_dialog.setWindowTitle(tr("导出配置文件"));
            file_dialog.setFileMode(QFileDialog::AnyFile);
            file_dialog.setNameFilter(tr("configuration (*.json)"));
            file_dialog.setViewMode(QFileDialog::Detail);
            file_dialog.selectFile("./" + tab_title + ".json");
            if (file_dialog.exec()) {
              QString file_name = file_dialog.selectedFiles()[0];
              if (!file_name.endsWith(".json")) {
                file_name = file_name.append(".json");
              }
              QFile file(file_name);
              file.open(QIODevice::WriteOnly);
              file.write(params.ToString().c_str());
              file.close();
            }
          });
  connect(ui->action_import_for_current_tab, &QAction::triggered, this,
          [this](bool checked) {
            auto tab = dynamic_cast<ImageStitch::ImageStitcherView *>(
                ui->tab_widget->currentWidget());
            QFileDialog file_dialog;
            file_dialog.setWindowTitle(tr("导入配置文件"));
            file_dialog.setFileMode(QFileDialog::ExistingFile);
            file_dialog.setNameFilter(tr("configuration (*.json)"));
            file_dialog.setViewMode(QFileDialog::Detail);
            if (file_dialog.exec()) {
              QString file_name = file_dialog.selectedFiles()[0];
              QFile file(file_name);
              if (file.exists()) {
                file.open(QIODevice::ReadOnly);
                auto data = file.readAll();
                file.close();
                Parameters params;
                params.FromString(data);
                tab->SetParams(params);
              }
            }
          });
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
    menu.addAction(tr("关闭"), [this, index]() {
      ui->tab_widget->tabCloseRequested(index);
    });
    menu.addAction(tr("重命名"), [this, index]() { TabBarEditor(index); });
  }
  menu.exec(ui->tab_widget->mapToGlobal(pos));
}

ImageStitch::ImageStitcherView *MainWindow::addTab(QString tabName) {
  ImageStitch::ImageStitcherView *imageStitchView =
      new ImageStitch::ImageStitcherView();
  ui->tab_widget->addTab(imageStitchView, tabName);
  imageStitchView->ShowMessage(tr("欢迎使用图像拼接软件（NekoIS）\n") +
                               tr("请拖拽或右键添加图片到待拼接图像列表中\n") +
                               tr("点击拼接可以完成图像拼接\n") +
                               tr("点击配置可以更改拼接算法"));
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