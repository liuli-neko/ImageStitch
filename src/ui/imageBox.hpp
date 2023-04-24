#pragma once

#include <QListView>
#include <QMap>
#include <QMenu>
#include <QPixmap>
#include <QWidget>

#include "imageView.hpp"

namespace ImageStitch {

class ImageItemModel : public QAbstractItemModel {
 public:
  ImageItemModel(QObject *parent = (QObject *)nullptr);
  ~ImageItemModel();
  QVariant data(const QModelIndex &_index,
                int role = Qt::DecorationRole) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &child = QModelIndex()) const override;
  bool setData(const QModelIndex &_index, const QVariant &value,
               int role = Qt::DecorationRole) override;
  bool insertRows(int row, int count,
                  const QModelIndex &parent = QModelIndex()) override;
  bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                const QModelIndex &destinationParent,
                int destinationChild) override;
  bool removeRows(int row, int count,
                  const QModelIndex &parent = QModelIndex()) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  QList<QPair<QString, QPixmap>> getItems(const int row, const int column);
  bool insertRows(const int row, const QList<QPair<QString, QPixmap>> &datas);
  QMap<int, QVariant> index2Item(const QModelIndex &index) const;
  bool addItem(const QString &file);
  bool addItem(const QImage &img, const QString &file_path = "");
  bool addItem(const QPixmap &pixmap, const QString &file_path = "");
  bool setItem(const int row, const QPair<QString, QPixmap> &data);
  bool removeItem(const int _index);
  QList<QPixmap> pixmapList();
  QList<QString> fileList();

 private:
  QList<QMap<int, QVariant>> _datas;
  int _hide_index;
  int _rows;
};
class ImageBox : public QListView {
  Q_OBJECT
 public:
  const int kDefaultIconWidth = 100;
  const int kDefaultIconHeight = 100;
  const int kDefaultWindowWidth = 250;
  const int kDefaultWindowHeight = 300;

 public:
  ImageBox(QWidget *parent = nullptr);
  ~ImageBox();

  // void contextMenuEvent(QContextMenuEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

 public slots:
  void handleDoubleClick(const QModelIndex &_index);
  void handleCustomContextMenu(const QPoint &pos);
  // void handleIndexesMoved(const QModelIndexList &indexes);
  void handleImageAdd();

 protected:
  void setupUi(const int width, const int height);

 private:
  QMenu *_menu;
  QAction *_menu_add;
  int _draging_index;
};

}  // namespace ImageStitch