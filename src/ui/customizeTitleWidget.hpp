#pragma once

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>

namespace ImageStitch {
/**
 * @brief TitleBar(llhsdmd-TitleBar ^-^)
 * 自定义窗体的标题栏，实现了窗体的拖动事件（只有标题栏能够拖动窗体）
 * 这里使用QToolBar代替（不知道有没有专门的标题栏）
 * 这里定义的标题栏构成参考自vscode等软件
 * 包含以下部分
 * 图标(Label) | 菜单(Menubar) | 空白 | 标题(Label) | 空白 |
 * 功能按钮(QToolButton)
 */
class TitleBar final : public QToolBar {
  Q_OBJECT
 public:
  /**
   * @brief Construct a new Title Bar object
   * 在这里使用layout构建了标题栏的大体结构。
   * @param parent
   */
  TitleBar(QWidget *parent = nullptr);
  /**
   * @brief Set the Title object
   *
   * @param title
   */
  void setTitle(const QString &title);
  /**
   * @brief Set the Icon object
   *
   * @param icon
   */
  void setIcon(const QPixmap &icon);
  /**
   * @brief 添加控件到标题栏中
   *
   * @param widget 控件
   * @param index ！！！
   * 你得自己保证插入的地方不影响现有布局，index包括现有部分，没有单独划分插入区域（不晓得怎么划比较好就干脆不划了吧
   * - by llhsdmd)
   * @param stretch layout接受的参数，表示对可分配空间的优先分配权重（
   * 空白为1，如果设为1会和其他空白均分可分配空间，默认控件都是0，即需要多少给多少。）
   * @param alignment 对齐方式
   */
  void addWidget(QWidget *widget, int index, int stretch = 0,
                 Qt::Alignment alignment = {0});
  /**
   * @brief Set the Resizable
   * 设置窗体是否可以resize（这里会影响最大化按钮）
   * @param resizeable
   */
  void setResizable(bool resizeable = true);
  /**
   * @brief Set the Movable
   * 设置窗体是否接受移动事件。
   * @param moveable
   */
  void setMovable(bool moveable = true);

  // 已下函数均继承自父类，用于定义标题栏需要的行为。
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;

  // void showEvent(QShowEvent *event) override;

 public:
  /**
   * @brief show full left screen
   *使窗口占满左半边屏幕
   */
  void showFullLeftScreen();
	/**
	 * @brief show full right screen
	 * 使窗口占满右半边屏幕
	 */
  void showFullRightScreen();
	/**
	 * @brief show normal
	 * 恢复窗口状态（主要恢复左半边和右半边满屏状态，Qt好像没有提供这两个）
	 */
  void showNormal();

 public slots:
  void MinimizedEvent(bool checked);
  void MaximizedEvent(bool checked);
  void CloseEvent(bool checked);

 private:
  QToolButton *toolButton_mini = nullptr;   // 最小化
  QToolButton *toolButton_max = nullptr;    // 最大化
  QToolButton *toolButton_close = nullptr;  // 关闭
  QHBoxLayout *hBoxLayout = nullptr;
  QLabel *titleLabel = nullptr;
  QLabel *titleIcon = nullptr;
  bool flag_resizable = true;
  bool flag_movable = true;
  bool flag_moving = false;
  bool flag_doubleClick = false;
  bool flag_welted = false;
  QPoint diff_position;
  QSize normal_size;
};

class CustomizeTitleWidget : public QWidget {
  Q_OBJECT
 public:
  CustomizeTitleWidget(QWidget *parent = nullptr);
  void addWidget(QWidget *widget, int index, int stretch = 0,
                 Qt::Alignment alignment = {0});
  void updateRegion(QMouseEvent *event);
  void resizeRegion(int marginTop, int marginBottom, int marginLeft,
                    int marginRight);
  void createShadow();

 public:
  bool event(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

 public:
  void setWindowTitle(const QString &title);
  void setWindowIcon(const QPixmap &icon);
  void addToolBar(QToolBar *toolBar);
  void addMenuBar(QMenuBar *menu);
  void setCentralWidget(QWidget *centralWidget);
  void setStatusBar(QStatusBar *statusBar);
  void showMinimized();
  void showMaximized();
  void showFullScreen();
  void showNormal();

 protected:
  TitleBar *title_bar = nullptr;
  QGroupBox *container_widget = nullptr;
  QWidget *center_widget = nullptr;
  QVBoxLayout *container_layout = nullptr;
  QVBoxLayout *main_layout = nullptr;
  QVBoxLayout *top_layout = nullptr;
  QVBoxLayout *center_layout = nullptr;
  QVBoxLayout *bottom_layout = nullptr;

 private:
  const int MARGIN_MIN_SIZE = -5;
  const int MARGIN_MAX_SIZE = 5;
  enum MovingDirection {
    NONE,
    BOTTOMRIGHT,
    TOPRIGHT,
    TOPLEFT,
    BOTTOMLEFT,
    DOWN,
    LEFT,
    RIGHT,
    UP
  };

 private:
  MovingDirection m_direction = NONE;
  QPoint press_pos;
  QPoint move_pos;
  bool flag_resizing = false;
  bool flag_pressed = false;
};
}  // namespace ImageStitch