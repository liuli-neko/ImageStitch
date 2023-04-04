#include <glog/logging.h>
#include <gtest/gtest.h>
#ifdef WIN
#include <windows.h>
#endif
#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <QWidget>
#include <cstdio>

class _Widget : public QLabel {
public:
  _Widget() {
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle("辅助窗口");
    setText("防止没有窗口被show,主进程exec函数卡死用窗口");
  }
  void showEvent(QShowEvent *event) override {
    QTimer::singleShot(10, this, [this]() { this->close(); });
  }
};

int main(int argc, char **argv) {
#ifdef WIN
  AllocConsole();                                     // debug console
  freopen_s((FILE **)stdout, "CONOUT$", "w", stdout); // just works
  freopen_s((FILE **)stderr, "CONOUT$", "w", stderr); // just works
#endif
  testing::InitGoogleTest(&argc, argv);
  QApplication a(argc, argv);
  RUN_ALL_TESTS();
  _Widget *widget = new _Widget();
  widget->show();
  a.exec();
  QLabel label;
  label.resize(400, 100);
  label.setWindowTitle("提示窗口");
  label.setFont(QFont("黑体", 32));
  label.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  label.setText("测试完成");
  label.show();
  return a.exec();
}