#include "NpyExplorer_Widget.hpp"
#include "NpyDatasetPreviewModel.hpp"
#include "ui_NpyExplorer_Widget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>

NpyExplorer_Widget::NpyExplorer_Widget(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::NpyExplorer_Widget),
      _data_manager(std::move(data_manager)),
      _preview_model(new NpyDatasetPreviewModel(this)) {
    ui->setupUi(this);

    // Set up preview table
    ui->previewTableView->setModel(_preview_model);
    ui->previewTableView->horizontalHeader()->setStretchLastSection(true);
    ui->previewTableView->verticalHeader()->setDefaultSectionSize(24);
    ui->previewTableView->setVisible(false);

    // Connect signals
    connect(ui->browseButton, &QPushButton::clicked,
            this, &NpyExplorer_Widget::_onBrowseClicked);
    connect(ui->refreshButton, &QPushButton::clicked,
            this, &NpyExplorer_Widget::_onRefreshClicked);

    // Connect preview model signals
    connect(_preview_model, &NpyDatasetPreviewModel::fileLoaded,
            this, [this](qint64 num_rows, int num_cols) {
                ui->previewStatusLabel->setText(
                    tr("Showing %1 rows x %2 columns (lazy-loaded)")
                    .arg(num_rows).arg(num_cols));
                ui->previewTableView->setVisible(true);
            });
    connect(_preview_model, &NpyDatasetPreviewModel::loadError,
            this, [this](QString const & msg) {
                ui->previewStatusLabel->setText(tr("Error: %1").arg(msg));
                ui->previewTableView->setVisible(false);
            });

    _clearDisplay();
}

NpyExplorer_Widget::~NpyExplorer_Widget() {
    delete ui;
}

bool NpyExplorer_Widget::loadFile(QString const & file_path) {
    if (file_path.isEmpty()) {
        emit errorOccurred(tr("No file path provided"));
        return false;
    }

    _clearDisplay();

    ui->previewStatusLabel->setText(tr("Loading..."));

    if (!_preview_model->loadFile(file_path)) {
        return false;
    }

    _current_file_path = file_path;
    ui->filePathEdit->setText(file_path);

    _updateInfoPanel(_preview_model->fileInfo());

    emit fileLoaded(file_path);
    return true;
}

void NpyExplorer_Widget::_onBrowseClicked() {
    QString file_path = QFileDialog::getOpenFileName(
        this,
        tr("Select NumPy File"),
        QString(),
        tr("NumPy Files (*.npy);;All Files (*)")
    );

    if (!file_path.isEmpty()) {
        loadFile(file_path);
    }
}

void NpyExplorer_Widget::_onRefreshClicked() {
    if (!_current_file_path.isEmpty()) {
        loadFile(_current_file_path);
    }
}

void NpyExplorer_Widget::_clearDisplay() {
    _preview_model->clear();
    ui->infoLabel->setText(tr("Select a .npy file to view its contents"));
    ui->previewStatusLabel->setText(tr("Select a file to preview its contents"));
    ui->previewTableView->setVisible(false);
}

void NpyExplorer_Widget::_updateInfoPanel(NpyFileInfo const & info) {
    // Build shape string
    QStringList shape_parts;
    for (auto dim : info.shape) {
        shape_parts.append(QString::number(dim));
    }
    QString shape_str = shape_parts.isEmpty() ? tr("scalar") : shape_parts.join(" x ");

    // Human-readable dtype
    QString type_str;
    switch (info.type_kind) {
        case 'f': type_str = tr("float%1").arg(info.item_size * 8); break;
        case 'i': type_str = tr("int%1").arg(info.item_size * 8); break;
        case 'u': type_str = tr("uint%1").arg(info.item_size * 8); break;
        case 'b': type_str = tr("bool"); break;
        case 'c': type_str = tr("complex%1").arg(info.item_size * 8); break;
        default:  type_str = tr("unknown (%1)").arg(info.dtype_str); break;
    }

    // Byte order string
    QString byte_order_str;
    switch (info.byte_order) {
        case '<': byte_order_str = tr("Little-endian"); break;
        case '>': byte_order_str = tr("Big-endian"); break;
        case '|': byte_order_str = tr("Not applicable"); break;
        default:  byte_order_str = tr("Unknown"); break;
    }

    // Memory layout
    QString layout_str = info.fortran_order ? tr("Fortran (column-major)") : tr("C (row-major)");

    // File size
    QFileInfo fi(_current_file_path);
    QString file_size_str;
    qint64 size = fi.size();
    if (size < 1024) {
        file_size_str = tr("%1 bytes").arg(size);
    } else if (size < 1024 * 1024) {
        file_size_str = tr("%1 KB").arg(static_cast<double>(size) / 1024.0, 0, 'f', 1);
    } else {
        file_size_str = tr("%1 MB").arg(static_cast<double>(size) / (1024.0 * 1024.0), 0, 'f', 1);
    }

    QString text = tr(
        "<b>File:</b> %1<br>"
        "<b>Data Type:</b> %2 (%3)<br>"
        "<b>Shape:</b> %4<br>"
        "<b>Total Elements:</b> %5<br>"
        "<b>Byte Order:</b> %6<br>"
        "<b>Memory Layout:</b> %7<br>"
        "<b>File Size:</b> %8"
    )
    .arg(QFileInfo(_current_file_path).fileName())
    .arg(type_str)
    .arg(info.dtype_str)
    .arg(shape_str)
    .arg(info.total_elements)
    .arg(byte_order_str)
    .arg(layout_str)
    .arg(file_size_str);

    ui->infoLabel->setText(text);
}
