#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "imageStitcherView.hpp"
#include "ui/customizeTitleWidget.hpp"

namespace Ui {
class MainWindow;
}

using namespace ImageStitch;

class MainWindow : public CustomizeTitleWidget {
  Q_OBJECT
 public:
  MainWindow(QWidget *parent = nullptr);
  virtual ~MainWindow();

 public:
  ImageStitcherView *addTab(QString tabName = "未命名");
  void handleTableDragEnter(QDragEnterEvent *mouseEvent);
  bool eventFilter(QObject *obj, QEvent *event) override;

 public slots:
  void TabMenu(const QPoint &pos);
  void TabBarEditor(const int index);
  void TabClose(const int index);

 private:
  Ui::MainWindow *ui;
};
#endif  // MAINWINDOW_H
