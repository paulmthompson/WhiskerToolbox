#include "HDF5Explorer_Widget.hpp"
#include "HDF5DatasetPreviewModel.hpp"
#include "ui_HDF5Explorer_Widget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTableView>
#include <QHeaderView>
#include <QMessageBox>
#include <QStyle>
#include <QApplication>

#include <H5Cpp.h>

#include <functional>

namespace {

// Custom data roles for storing object info in tree items
constexpr int ROLE_FULL_PATH = Qt::UserRole;
constexpr int ROLE_IS_GROUP = Qt::UserRole + 1;
constexpr int ROLE_DATA_TYPE = Qt::UserRole + 2;
constexpr int ROLE_DIMENSIONS = Qt::UserRole + 3;
constexpr int ROLE_NUM_ATTRIBUTES = Qt::UserRole + 4;

/**
 * @brief Get a human-readable string for HDF5 data type
 */
QString getDataTypeString(H5::DataType const & dtype) {
    H5T_class_t type_class = dtype.getClass();
    
    switch (type_class) {
        case H5T_INTEGER: {
            H5::IntType int_type(dtype.getId());
            size_t size = int_type.getSize();
            H5T_sign_t sign = int_type.getSign();
            QString sign_str = (sign == H5T_SGN_NONE) ? "uint" : "int";
            return QString("%1%2").arg(sign_str).arg(size * 8);
        }
        case H5T_FLOAT: {
            size_t size = dtype.getSize();
            return QString("float%1").arg(size * 8);
        }
        case H5T_STRING:
            return "string";
        case H5T_COMPOUND:
            return "compound";
        case H5T_ARRAY:
            return "array";
        case H5T_VLEN:
            return "variable-length";
        case H5T_ENUM:
            return "enum";
        case H5T_REFERENCE:
            return "reference";
        case H5T_OPAQUE:
            return "opaque";
        default:
            return "unknown";
    }
}

/**
 * @brief Get dimensions as a string list
 */
QStringList getDimensionsString(H5::DataSpace const & space) {
    QStringList dims;
    
    if (space.getSimpleExtentType() == H5S_SCALAR) {
        dims.append("scalar");
        return dims;
    }
    
    int ndims = space.getSimpleExtentNdims();
    std::vector<hsize_t> dim_sizes(static_cast<size_t>(ndims));
    space.getSimpleExtentDims(dim_sizes.data());
    
    for (size_t i = 0; i < dim_sizes.size(); ++i) {
        dims.append(QString::number(dim_sizes[i]));
    }
    
    return dims;
}

/**
 * @brief Recursively add HDF5 objects to tree using H5::Group
 */
void addObjectsToTree(H5::Group const & parent, QTreeWidgetItem * parent_item, 
                      QString const & parent_path) {
    hsize_t num_objs = parent.getNumObjs();
    
    for (hsize_t i = 0; i < num_objs; ++i) {
        std::string obj_name = parent.getObjnameByIdx(i);
        QString q_name = QString::fromStdString(obj_name);
        QString full_path = parent_path.isEmpty() ? "/" + q_name : parent_path + "/" + q_name;
        
        H5G_obj_t obj_type = parent.getObjTypeByIdx(i);
        
        auto * item = new QTreeWidgetItem(parent_item);
        item->setText(0, q_name);
        item->setData(0, ROLE_FULL_PATH, full_path);
        
        if (obj_type == H5G_GROUP) {
            item->setText(1, "Group");
            item->setData(0, ROLE_IS_GROUP, true);
            item->setIcon(0, QIcon::fromTheme("folder", 
                QApplication::style()->standardIcon(QStyle::SP_DirIcon)));
            
            // Get number of attributes
            H5::Group group = parent.openGroup(obj_name);
            int num_attrs = static_cast<int>(group.getNumAttrs());
            item->setData(0, ROLE_NUM_ATTRIBUTES, num_attrs);
            
            // Recurse into group
            addObjectsToTree(group, item, full_path);
            group.close();
        } else if (obj_type == H5G_DATASET) {
            H5::DataSet dataset = parent.openDataSet(obj_name);
            H5::DataType dtype = dataset.getDataType();
            H5::DataSpace space = dataset.getSpace();
            
            QString type_str = getDataTypeString(dtype);
            QStringList dims = getDimensionsString(space);
            int num_attrs = static_cast<int>(dataset.getNumAttrs());
            
            item->setText(1, "Dataset");
            item->setText(2, type_str);
            item->setText(3, dims.join(" × "));
            item->setData(0, ROLE_IS_GROUP, false);
            item->setData(0, ROLE_DATA_TYPE, type_str);
            item->setData(0, ROLE_DIMENSIONS, dims);
            item->setData(0, ROLE_NUM_ATTRIBUTES, num_attrs);
            item->setIcon(0, QIcon::fromTheme("text-x-generic",
                QApplication::style()->standardIcon(QStyle::SP_FileIcon)));
            
            dataset.close();
        }
    }
}

/**
 * @brief Add HDF5 file root objects to tree using H5::H5File
 */
void addRootObjectsToTree(H5::H5File const & file, QTreeWidgetItem * parent_item) {
    hsize_t num_objs = file.getNumObjs();
    
    for (hsize_t i = 0; i < num_objs; ++i) {
        std::string obj_name = file.getObjnameByIdx(i);
        QString q_name = QString::fromStdString(obj_name);
        QString full_path = "/" + q_name;
        
        H5G_obj_t obj_type = file.getObjTypeByIdx(i);
        
        auto * item = new QTreeWidgetItem(parent_item);
        item->setText(0, q_name);
        item->setData(0, ROLE_FULL_PATH, full_path);
        
        if (obj_type == H5G_GROUP) {
            item->setText(1, "Group");
            item->setData(0, ROLE_IS_GROUP, true);
            item->setIcon(0, QIcon::fromTheme("folder", 
                QApplication::style()->standardIcon(QStyle::SP_DirIcon)));
            
            // Get number of attributes
            H5::Group group = file.openGroup(obj_name);
            int num_attrs = static_cast<int>(group.getNumAttrs());
            item->setData(0, ROLE_NUM_ATTRIBUTES, num_attrs);
            
            // Recurse into group
            addObjectsToTree(group, item, full_path);
            group.close();
        } else if (obj_type == H5G_DATASET) {
            H5::DataSet dataset = file.openDataSet(obj_name);
            H5::DataType dtype = dataset.getDataType();
            H5::DataSpace space = dataset.getSpace();
            
            QString type_str = getDataTypeString(dtype);
            QStringList dims = getDimensionsString(space);
            int num_attrs = static_cast<int>(dataset.getNumAttrs());
            
            item->setText(1, "Dataset");
            item->setText(2, type_str);
            item->setText(3, dims.join(" × "));
            item->setData(0, ROLE_IS_GROUP, false);
            item->setData(0, ROLE_DATA_TYPE, type_str);
            item->setData(0, ROLE_DIMENSIONS, dims);
            item->setData(0, ROLE_NUM_ATTRIBUTES, num_attrs);
            item->setIcon(0, QIcon::fromTheme("text-x-generic",
                QApplication::style()->standardIcon(QStyle::SP_FileIcon)));
            
            dataset.close();
        }
    }
}

} // anonymous namespace


HDF5Explorer_Widget::HDF5Explorer_Widget(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HDF5Explorer_Widget),
      _data_manager(std::move(data_manager)),
      _preview_model(new HDF5DatasetPreviewModel(this)) {
    ui->setupUi(this);

    // Set up tree widget columns
    ui->treeWidget->setHeaderLabels({"Name", "Type", "Data Type", "Dimensions"});
    ui->treeWidget->setColumnWidth(0, 200);
    ui->treeWidget->setColumnWidth(1, 80);
    ui->treeWidget->setColumnWidth(2, 100);
    ui->treeWidget->setColumnWidth(3, 150);

    // Set up preview table
    ui->previewTableView->setModel(_preview_model);
    ui->previewTableView->horizontalHeader()->setStretchLastSection(true);
    ui->previewTableView->verticalHeader()->setDefaultSectionSize(24);
    ui->previewTableView->setVisible(false);  // Hidden until a dataset is selected

    // Connect signals
    connect(ui->browseButton, &QPushButton::clicked,
            this, &HDF5Explorer_Widget::_onBrowseClicked);
    connect(ui->refreshButton, &QPushButton::clicked,
            this, &HDF5Explorer_Widget::_onRefreshClicked);
    connect(ui->treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &HDF5Explorer_Widget::_onTreeSelectionChanged);
    connect(ui->treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &HDF5Explorer_Widget::_onTreeItemDoubleClicked);
    
    // Connect preview model signals
    connect(_preview_model, &HDF5DatasetPreviewModel::datasetLoaded,
            this, [this](qint64 num_rows, int num_cols) {
                ui->previewStatusLabel->setText(
                    tr("Showing %1 rows × %2 columns (lazy-loaded)")
                    .arg(num_rows).arg(num_cols));
                ui->previewTableView->setVisible(true);
            });
    connect(_preview_model, &HDF5DatasetPreviewModel::loadError,
            this, [this](QString const & msg) {
                ui->previewStatusLabel->setText(tr("Error: %1").arg(msg));
                ui->previewTableView->setVisible(false);
            });

    // Initial state
    _clearDisplay();
}

HDF5Explorer_Widget::~HDF5Explorer_Widget() {
    delete ui;
}

bool HDF5Explorer_Widget::loadFile(QString const & file_path) {
    if (file_path.isEmpty()) {
        emit errorOccurred(tr("No file path provided"));
        return false;
    }

    _clearDisplay();
    
    if (!_populateTree(file_path)) {
        return false;
    }

    _current_file_path = file_path;
    ui->filePathEdit->setText(file_path);
    emit fileLoaded(file_path);
    return true;
}

QString HDF5Explorer_Widget::selectedDatasetPath() const {
    auto * item = ui->treeWidget->currentItem();
    if (!item) {
        return QString();
    }
    
    bool is_group = item->data(0, ROLE_IS_GROUP).toBool();
    if (is_group) {
        return QString();  // Don't return path for groups
    }
    
    return item->data(0, ROLE_FULL_PATH).toString();
}

HDF5ObjectInfo HDF5Explorer_Widget::selectedObjectInfo() const {
    auto * item = ui->treeWidget->currentItem();
    if (!item) {
        return HDF5ObjectInfo{};
    }
    return _getInfoFromItem(item);
}

void HDF5Explorer_Widget::_onBrowseClicked() {
    QString file_path = QFileDialog::getOpenFileName(
        this,
        tr("Select HDF5 File"),
        QString(),
        tr("HDF5 Files (*.h5 *.hdf5 *.hdf);;All Files (*)")
    );

    if (!file_path.isEmpty()) {
        loadFile(file_path);
    }
}

void HDF5Explorer_Widget::_onTreeSelectionChanged() {
    auto * item = ui->treeWidget->currentItem();
    if (!item) {
        ui->infoLabel->setText(tr("No selection"));
        _clearPreviewTable();
        return;
    }

    HDF5ObjectInfo info = _getInfoFromItem(item);
    _updateInfoPanel(info);

    if (!info.is_group) {
        _updatePreviewTable(info);
        emit datasetSelected(info.full_path);
    } else {
        _clearPreviewTable();
    }
}

void HDF5Explorer_Widget::_onTreeItemDoubleClicked(QTreeWidgetItem * item, int /* column */) {
    if (!item) {
        return;
    }

    HDF5ObjectInfo info = _getInfoFromItem(item);
    
    // Only emit for datasets, not groups
    if (!info.is_group) {
        emit datasetActivated(info.full_path, info);
    }
}

void HDF5Explorer_Widget::_onRefreshClicked() {
    if (!_current_file_path.isEmpty()) {
        loadFile(_current_file_path);
    }
}

void HDF5Explorer_Widget::_clearDisplay() {
    ui->treeWidget->clear();
    ui->infoLabel->setText(tr("Select a file to browse its structure"));
    _clearPreviewTable();
}

void HDF5Explorer_Widget::_updateInfoPanel(HDF5ObjectInfo const & info) {
    QString text;
    
    if (info.is_group) {
        text = tr("<b>Group:</b> %1<br>"
                  "<b>Path:</b> %2<br>"
                  "<b>Attributes:</b> %3")
               .arg(info.name)
               .arg(info.full_path)
               .arg(info.num_attributes);
    } else {
        QString dims_str = info.dimensions.isEmpty() ? 
                           tr("(unknown)") : info.dimensions.join(" × ");
        text = tr("<b>Dataset:</b> %1<br>"
                  "<b>Path:</b> %2<br>"
                  "<b>Type:</b> %3<br>"
                  "<b>Dimensions:</b> %4<br>"
                  "<b>Attributes:</b> %5")
               .arg(info.name)
               .arg(info.full_path)
               .arg(info.data_type)
               .arg(dims_str)
               .arg(info.num_attributes);
    }
    
    ui->infoLabel->setText(text);
}

void HDF5Explorer_Widget::_updatePreviewTable(HDF5ObjectInfo const & info) {
    if (_current_file_path.isEmpty() || info.full_path.isEmpty()) {
        _clearPreviewTable();
        return;
    }
    
    ui->previewStatusLabel->setText(tr("Loading preview..."));
    _preview_model->loadDataset(_current_file_path, info.full_path);
}

void HDF5Explorer_Widget::_clearPreviewTable() {
    _preview_model->clear();
    ui->previewStatusLabel->setText(tr("Select a dataset to preview its contents"));
    ui->previewTableView->setVisible(false);
}

bool HDF5Explorer_Widget::_populateTree(QString const & file_path) {
    try {
        std::string path_str = file_path.toStdString();
        H5::H5File file(path_str.c_str(), H5F_ACC_RDONLY);

        // Create root item
        auto * root = new QTreeWidgetItem(ui->treeWidget);
        root->setText(0, QFileInfo(file_path).fileName());
        root->setText(1, "File");
        root->setData(0, ROLE_FULL_PATH, "/");
        root->setData(0, ROLE_IS_GROUP, true);
        root->setData(0, ROLE_NUM_ATTRIBUTES, static_cast<int>(file.getNumAttrs()));
        root->setIcon(0, QIcon::fromTheme("package-x-generic",
            QApplication::style()->standardIcon(QStyle::SP_FileIcon)));
        
        // Recursively add all objects starting from file root
        addRootObjectsToTree(file, root);
        
        // Expand root
        root->setExpanded(true);
        
        file.close();
        return true;
        
    } catch (H5::FileIException const & e) {
        QString msg = tr("Failed to open HDF5 file: %1\n%2")
                      .arg(file_path)
                      .arg(QString::fromStdString(e.getDetailMsg()));
        emit errorOccurred(msg);
        QMessageBox::warning(this, tr("HDF5 Error"), msg);
        return false;
        
    } catch (H5::Exception const & e) {
        QString msg = tr("HDF5 error while reading file: %1\n%2")
                      .arg(file_path)
                      .arg(QString::fromStdString(e.getDetailMsg()));
        emit errorOccurred(msg);
        QMessageBox::warning(this, tr("HDF5 Error"), msg);
        return false;
    }
}

HDF5ObjectInfo HDF5Explorer_Widget::_getInfoFromItem(QTreeWidgetItem * item) const {
    HDF5ObjectInfo info;
    
    if (!item) {
        return info;
    }
    
    info.name = item->text(0);
    info.full_path = item->data(0, ROLE_FULL_PATH).toString();
    info.is_group = item->data(0, ROLE_IS_GROUP).toBool();
    info.data_type = item->data(0, ROLE_DATA_TYPE).toString();
    info.dimensions = item->data(0, ROLE_DIMENSIONS).toStringList();
    info.num_attributes = item->data(0, ROLE_NUM_ATTRIBUTES).toInt();
    
    return info;
}
