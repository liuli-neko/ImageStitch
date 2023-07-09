#include "imageBox.hpp"

#include <QDebug>
#include <QDrag>
#include <QFileDialog>
#include <QMimeData>
#include <QWhatsThis>
#include <algorithm>
#include <filesystem>
#include <vector>

namespace ImageStitch {
ImageItemModel::ImageItemModel(QObject *parent) : QAbstractItemModel(parent) {
  _rows = 0;
}
ImageItemModel::~ImageItemModel() {}
QVariant ImageItemModel::data(const QModelIndex &_index, int role) const {
  if (!_index.isValid() || _index.row() < 0 || _index.column() != 0 ||
      _index.row() >= _rows) {
    return QVariant();
  }
  switch (role) {
    case Qt::DecorationRole:
      return QIcon(
          _datas.at(_index.row())[(int)Qt::DecorationRole].value<QPixmap>());
    case Qt::DisplayRole:
      return QString(
          QFileInfo(
              _datas.at(_index.row())[(int)Qt::DisplayRole].value<QString>())
              .baseName());
    case Qt::ToolTipRole:
      return _datas.at(_index.row())[(int)Qt::DisplayRole].value<QString>();
  }
  return QVariant();
}

int ImageItemModel::columnCount(const QModelIndex &parent) const { return 1; }

int ImageItemModel::rowCount(const QModelIndex &parent) const { return _rows; }

QModelIndex ImageItemModel::index(int row, int column,
                                  const QModelIndex &parent) const {
  if (row < 0 || row >= rowCount() || column != 0) {
    return QModelIndex();
  }
  return createIndex(row, 0, (void *)&_datas[row]);
}
QModelIndex ImageItemModel::parent(const QModelIndex &child) const {
  return QModelIndex();
}
QList<QPair<QString, QPixmap>> ImageItemModel::getItems(const int row,
                                                        const int column) {
  QList<QPair<QString, QPixmap>> items;
  if (row < 0 || row + column > rowCount() || column < 1) {
    return items;
  }
  auto it = _datas.begin() + row;
  for (int i = 0; i < column; ++i, ++it) {
    items.append(QPair((*it)[Qt::DisplayRole].value<QString>(),
                       (*it)[Qt::DecorationRole].value<QPixmap>()));
  }
  return items;
}
bool ImageItemModel::insertRows(const int row,
                                const QList<QPair<QString, QPixmap>> &datas) {
  int count = datas.size();
  if (!insertRows(row, count)) {
    return false;
  }
  for (int i = 0; i < count; ++i) {
    setItem(i + row, QPair(datas[i].first, datas[i].second));
  }
  return true;
}
bool ImageItemModel::setData(const QModelIndex &_index, const QVariant &value,
                             int role) {
  if (_index.column() != 0 || _index.row() < 0 || _index.row() >= rowCount()) {
    return false;
  }
  QModelIndex idx;
  switch (role) {
    case Qt::DecorationRole:
      _datas[_index.row()][Qt::DecorationRole] = value;
      idx = createIndex(_index.row(), 0, (void *)&_datas[_index.row()]);
      emit dataChanged(idx, idx, {role});
      return true;
    case Qt::DisplayRole:
      _datas[_index.row()][Qt::DisplayRole] = value;
      idx = createIndex(_index.row(), 0, (void *)&_datas[_index.row()]);
      emit dataChanged(idx, idx, {role});
      return true;
  }
  return false;
}
bool ImageItemModel::moveRows(const QModelIndex &sourceParent, int sourceRow,
                              int count, const QModelIndex &destinationParent,
                              int destinationChild) {
  if (sourceRow < 0 || sourceRow + count > rowCount() || count < 1 ||
      destinationChild >= rowCount() ||
      (destinationChild > sourceRow &&
       destinationChild < rowCount() - sourceRow - count)) {
    return false;
  }
  beginMoveRows(sourceParent, sourceRow, sourceRow + count, destinationParent,
                destinationChild);
  auto items = getItems(sourceRow, count);
  if (!removeRows(sourceRow, count)) {
    return false;
  }
  if (!insertRows(destinationChild, items)) {
    return false;
  }
  endMoveRows();
  return true;
}
bool ImageItemModel::insertRows(int row, int count, const QModelIndex &parent) {
  if (row < 0 || row > rowCount(parent) || count < 1) {
    return false;
  }
  beginInsertRows(parent, row, row + count - 1);
  if (_rows == 0) {
    int i = 0;
    while (i < count) {
      _datas.push_back(QMap<int, QVariant>());
      ++i;
    }
    _rows = count;
  } else {
    auto it = _datas.begin() + row;
    int i = 0;
    while (i < count) {
      it = _datas.insert(it, QMap<int, QVariant>());
      ++i;
    }
    _rows += count;
  }
  endInsertRows();
  return true;
}
bool ImageItemModel::removeRows(int row, int count, const QModelIndex &parent) {
  if (row < 0 || row > rowCount(parent) || count < 1 ||
      row + count > rowCount(parent)) {
    qWarning() << "Invalid row";
    return false;
  }
  beginRemoveRows(parent, row, row + count - 1);
  auto it = _datas.begin() + row;
  int i = 0;
  while (i < count) {
    it = _datas.erase(it);
    ++i;
  }
  _rows -= count;
  endRemoveRows();
  return true;
}
Qt::ItemFlags ImageItemModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = Qt::NoItemFlags;

  if (index.isValid()) {
    return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled |
           Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  } else {
    return defaultFlags;
  }
}
QMap<int, QVariant> ImageItemModel::index2Item(const QModelIndex &index) const {
  if (index.isValid() && index.row() < rowCount()) {
    return _datas[index.row()];
  }
  return QMap<int, QVariant>();
}
QMimeData *ImageItemModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *data = new QMimeData;
  if (indexes.size() > 1) {
    qWarning() << "不支持同时拖拽多个图像";
    return data;
  } else if (indexes.size() < 1 || !indexes.at(0).isValid()) {
    qWarning() << "没有图像被选中";
    return data;
  } else {
    qDebug() << "mimeData";
    auto item = index2Item(indexes.at(0));
    data->setText(item[Qt::DisplayRole].value<QString>());
    data->setImageData(item[Qt::DecorationRole].value<QPixmap>());
    return data;
  }
}

bool ImageItemModel::addItem(const QString &file) {
  if (!QFileInfo(file).isFile()) {
    qWarning() << "File " << file << " does not exist";
    return false;
  }
  return addItem(QImage(file), file);
}
bool ImageItemModel::addItem(const QImage &img, const QString &file_path) {
  return addItem(QPixmap::fromImage(img), file_path);
}
bool ImageItemModel::addItem(const QPixmap &pixmap, const QString &file_path) {
  return insertRow(rowCount()) &&
         setItem(rowCount() - 1, QPair(file_path, pixmap));
}
bool ImageItemModel::setItem(const int row,
                             const QPair<QString, QPixmap> &data) {
  return setData(index(row, 0, QModelIndex()), data.first, Qt::DisplayRole) &&
         setData(index(row, 0, QModelIndex()), data.second, Qt::DecorationRole);
}

bool ImageItemModel::removeItem(const int _index) { return removeRow(_index); }
QList<QPixmap> ImageItemModel::pixmapList() {
  QList<QPixmap> pixmapList;
  for (auto &data : _datas) {
    pixmapList.append(data[Qt::DecorationRole].value<QPixmap>());
  }
  return pixmapList;
}
QList<QString> ImageItemModel::fileList() {
  QList<QString> fileList;
  for (auto &data : _datas) {
    fileList.append(data[Qt::DisplayRole].value<QString>());
  }
  return fileList;
}
ImageBox::ImageBox(QWidget *parent) : QListView(parent) {
  setupUi(kDefaultWindowWidth, kDefaultWindowHeight);
}
ImageBox::~ImageBox() {
  // TODO(llhsdmd): to do
}
void ImageBox::setupUi(const int width, const int height) {
  resize(width, height);
  setViewMode(QListView::ListMode);
  setIconSize(QSize(kDefaultIconWidth, kDefaultIconHeight));
  setLayoutDirection(Qt::LayoutDirectionAuto);
  setContextMenuPolicy(Qt::CustomContextMenu);
  setDragDropMode(QListView::DragDropMode::DropOnly);
  setAcceptDrops(true);
  // setDragDropOverwriteMode(true);
  _menu = new QMenu(this);
  _menu_add = _menu->addAction(tr("添加"));
  _draging_index = -1;
  connect(this, &QListView::doubleClicked, this, &ImageBox::handleDoubleClick);
  connect(this, &QListView::customContextMenuRequested, this,
          &ImageBox::handleCustomContextMenu);
  connect(_menu_add, &QAction::triggered, this, &ImageBox::handleImageAdd);
}
void ImageBox::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->accept();
  } else if (event->mimeData()->hasText()) {
    event->accept();
  } else if (event->mimeData()->hasImage()) {
    event->accept();
  }
  QWidget::dragEnterEvent(event);
}

void ImageBox::dragMoveEvent(QDragMoveEvent *event) {
  QWidget::dragMoveEvent(event);
}
void ImageBox::dropEvent(QDropEvent *event) {
  qDebug() << "ImageBox::dropEvent";
  auto index = indexAt(event->pos()).row();
  if (event->mimeData()->hasImage()) {
    qDebug() << "ImageBox::dropEvent image";
    auto imageItemModel = dynamic_cast<ImageItemModel *>(model());
    const QPixmap pixmap = event->mimeData()->imageData().value<QPixmap>();
    QString text = "";
    if (event->mimeData()->hasText()) {
      text = event->mimeData()->text();
    }
    imageItemModel->addItem(pixmap, text);
    if (index > -1) {
      imageItemModel->moveRow(QModelIndex(), imageItemModel->rowCount() - 1,
                              QModelIndex(), index);
    }
    return;
  }
  if (event->mimeData()->hasText()) {
    auto text = event->mimeData()->text();
    if (!text.startsWith("file:///") && !text.startsWith("http")) {
      if (!(text.endsWith(".jpg") || text.endsWith(".png") ||
            text.endsWith(".jpeg"))) {
        qWarning() << "文件后缀不识别";
        return;
      }
      auto imageItemModel = dynamic_cast<ImageItemModel *>(model());
      imageItemModel->addItem(text);
      if (index > -1) {
        imageItemModel->moveRow(QModelIndex(), imageItemModel->rowCount() - 1,
                                QModelIndex(), index);
      }
      return;
    }
  }
  if (event->mimeData()->hasUrls()) {
    auto urls = event->mimeData()->urls();
    for (const auto &url : urls) {
      auto path = url.toLocalFile();
      auto imageItemModel = dynamic_cast<ImageItemModel *>(model());
      imageItemModel->addItem(path);
      if (index > -1) {
        imageItemModel->moveRow(QModelIndex(), imageItemModel->rowCount() - 1,
                                QModelIndex(), index);
      }
    }
    return;
  }
}

void ImageBox::handleDoubleClick(const QModelIndex &_index) {
  auto data = dynamic_cast<ImageItemModel *>(model())->index2Item(_index);
  auto pixmap = data[Qt::DecorationRole];
  CustomizeTitleWidget *widget = new CustomizeTitleWidget();
  connect(this, &CustomizeTitleWidget::destroyed, widget,
          [widget](QObject *) { widget->close(); });
  ImageView *imageView = new ImageView(pixmap.value<QPixmap>());
  widget->setCentralWidget(imageView);
  widget->setWindowTitle(
      QFileInfo(data[Qt::DisplayRole].value<QString>()).baseName());
  widget->setAttribute(Qt::WA_DeleteOnClose, true);
  QToolButton *on_top = new QToolButton();
  widget->addWidget(on_top, 4);
  QImage top_icon("./top.jpg");
  if (top_icon.isNull()) {
    on_top->setText(tr("置顶"));
  } else {
    on_top->setIcon(QPixmap::fromImage(top_icon));
  }
  auto default_flags = widget->windowFlags();
  connect(
      on_top, &QToolButton::clicked, widget,
      [widget, on_top, top_icon, default_flags]() {
        if (widget->windowFlags() & Qt::WindowStaysOnTopHint) {
          widget->setWindowFlags(default_flags);
          if (top_icon.isNull()) {
            on_top->setText(tr("置顶"));
          } else {
            on_top->setIcon(QPixmap::fromImage(top_icon));
          }
          widget->show();
        } else {
          if (top_icon.isNull()) {
            on_top->setText(tr("取消置顶"));
          } else {
            on_top->setIcon(QPixmap::fromImage(top_icon.mirrored(false, true)));
          }
          widget->setWindowFlags(default_flags | Qt::WindowStaysOnTopHint);
          widget->show();
        }
      });
  widget->show();
}
void ImageBox::handleCustomContextMenu(const QPoint &pos) {
  QModelIndex index = indexAt(pos);
  if (index.isValid()) {
    // 设置删除菜单
    QAction *del = _menu->addAction(tr("删除"));
    connect(del, &QAction::triggered, this,
            [this, &index]() { model()->removeRow(index.row()); });
    // 设置保存菜单
    QAction *save = _menu->addAction(tr("另存为"));
    connect(save, &QAction::triggered, this, [this, &index]() {
      auto file_path = model()->data(index, Qt::DisplayRole);
      auto pixmap = model()->data(index, Qt::DecorationRole);
      QString fileName = QFileDialog::getSaveFileName(
          this,
          tr("另存文件"),               // dialog title
          file_path.value<QString>(),   // default directory
          tr("Images (*.png *.jpg)"));  // filter files by extension
      pixmap.value<QPixmap>().save(fileName);
    });
    // 唤出菜单
    _menu->exec(mapToGlobal(pos));
    // 删除添加的菜单项
    _menu->removeAction(save);
    _menu->removeAction(del);
  } else {
    _menu->exec(mapToGlobal(pos));
  }
}

void ImageBox::handleImageAdd() {
  QStringList fileNames = QFileDialog::getOpenFileNames(
      this,
      tr("添加图像"),                                  // dialog title
      "./",                                            // default directory
      "Images (*.png *.jpg *.jpeg);;All File (*.*)");  // filter files by
                                                       // extension
  auto imageItemModel = dynamic_cast<ImageItemModel *>(model());
  for (const auto &file : fileNames) {
    imageItemModel->addItem(file);
  }
}

}  // namespace ImageStitch