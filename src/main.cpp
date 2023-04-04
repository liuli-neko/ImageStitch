#ifdef WIN
#include <windows.h>
#endif
#include <QApplication>
#include <QStyleFactory>

#include "mainWindow.h"

int main(int argc, char *argv[]) {
#ifdef WIN
  AllocConsole();                                      // debug console
  freopen_s((FILE **)stdout, "CONOUT$", "w", stdout);  // just works
  freopen_s((FILE **)stderr, "CONOUT$", "w", stderr);  // just works
#endif
  QApplication a(argc, argv);
  MainWindow w;
  w.show();
  a.setFont(QFont("黑体", 12));
  QApplication::setStyle(QStyleFactory::create("fusion"));
  return a.exec();
}
