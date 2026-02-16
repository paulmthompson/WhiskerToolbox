#include "BinaryExplorer_Widget.hpp"
#include "BinaryDatasetPreviewModel.hpp"
#include "ui_BinaryExplorer_Widget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>

BinaryExplorer_Widget::BinaryExplorer_Widget(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::BinaryExplorer_Widget),
      _data_manager(std::move(data_manager)),
      _preview_model(new BinaryDatasetPreviewModel(this)) {
    ui->setupUi(this);

    // Set up combos
    _populateDataTypeCombo();
    _populateByteOrderCombo();

    // Set up preview table
    ui->previewTableView->setModel(_preview_model);
    ui->previewTableView->horizontalHeader()->setStretchLastSection(true);
    ui->previewTableView->verticalHeader()->setDefaultSectionSize(24);
    ui->previewTableView->setVisible(false);

    // Connections: file selection
    connect(ui->browseButton, &QPushButton::clicked,
            this, &BinaryExplorer_Widget::_onBrowseClicked);
    connect(ui->applyButton, &QPushButton::clicked,
            this, &BinaryExplorer_Widget::_onApplyClicked);

    // Connections: config changes trigger live reload
    connect(ui->headerSizeSpinBox, &QSpinBox::valueChanged,
            this, &BinaryExplorer_Widget::_onConfigChanged);
    connect(ui->dataTypeCombo, &QComboBox::currentIndexChanged,
            this, &BinaryExplorer_Widget::_onConfigChanged);
    connect(ui->numChannelsSpinBox, &QSpinBox::valueChanged,
            this, &BinaryExplorer_Widget::_onConfigChanged);
    connect(ui->byteOrderCombo, &QComboBox::currentIndexChanged,
            this, &BinaryExplorer_Widget::_onConfigChanged);
    connect(ui->bitwiseCheckBox, &QCheckBox::toggled,
            this, &BinaryExplorer_Widget::_onConfigChanged);

    // Connections: model signals
    connect(_preview_model, &BinaryDatasetPreviewModel::fileLoaded,
            this, [this](qint64 num_rows, int num_cols) {
                ui->previewStatusLabel->setText(
                    tr("Showing %1 rows x %2 columns (lazy-loaded)")
                    .arg(num_rows).arg(num_cols));
                ui->previewTableView->setVisible(true);
            });
    connect(_preview_model, &BinaryDatasetPreviewModel::loadError,
            this, [this](QString const & msg) {
                ui->previewStatusLabel->setText(tr("Error: %1").arg(msg));
                ui->previewTableView->setVisible(false);
            });

    _clearDisplay();
}

BinaryExplorer_Widget::~BinaryExplorer_Widget() {
    delete ui;
}

bool BinaryExplorer_Widget::loadFile(QString const & file_path) {
    if (file_path.isEmpty()) {
        emit errorOccurred(tr("No file path provided"));
        return false;
    }

    _clearDisplay();
    _current_file_path = file_path;
    ui->filePathEdit->setText(file_path);

    ui->previewStatusLabel->setText(tr("Loading..."));

    auto config = _buildConfigFromUI();
    if (!_preview_model->loadFile(file_path, config)) {
        return false;
    }

    _updateInfoPanel(_preview_model->fileInfo());

    emit fileLoaded(file_path);
    return true;
}

void BinaryExplorer_Widget::_onBrowseClicked() {
    QString file_path = QFileDialog::getOpenFileName(
        this,
        tr("Select Binary File"),
        QString(),
        tr("Binary Files (*.bin *.dat *.raw *.binary);;All Files (*)")
    );

    if (!file_path.isEmpty()) {
        loadFile(file_path);
    }
}

void BinaryExplorer_Widget::_onApplyClicked() {
    if (!_current_file_path.isEmpty()) {
        loadFile(_current_file_path);
    }
}

void BinaryExplorer_Widget::_onConfigChanged() {
    if (_current_file_path.isEmpty() || !_preview_model->hasData()) {
        return;
    }

    auto config = _buildConfigFromUI();
    if (_preview_model->reloadWithConfig(config)) {
        _updateInfoPanel(_preview_model->fileInfo());
    }
}

void BinaryExplorer_Widget::_clearDisplay() {
    _preview_model->clear();
    ui->infoLabel->setText(tr("Select a binary file to view its contents"));
    ui->previewStatusLabel->setText(tr("Select a file to preview its contents"));
    ui->previewTableView->setVisible(false);
}

BinaryParseConfig BinaryExplorer_Widget::_buildConfigFromUI() const {
    BinaryParseConfig config;
    config.header_size = ui->headerSizeSpinBox->value();
    config.data_type = static_cast<BinaryDataType>(ui->dataTypeCombo->currentData().toInt());
    config.num_channels = ui->numChannelsSpinBox->value();
    config.byte_order = static_cast<BinaryByteOrder>(ui->byteOrderCombo->currentData().toInt());
    config.bitwise_expand = ui->bitwiseCheckBox->isChecked();
    return config;
}

void BinaryExplorer_Widget::_updateInfoPanel(BinaryFileInfo const & info) {
    QFileInfo fi(_current_file_path);

    QString file_size_str;
    qint64 size = info.file_size;
    if (size < 1024) {
        file_size_str = tr("%1 bytes").arg(size);
    } else if (size < 1024 * 1024) {
        file_size_str = tr("%1 KB").arg(static_cast<double>(size) / 1024.0, 0, 'f', 1);
    } else {
        file_size_str = tr("%1 MB").arg(static_cast<double>(size) / (1024.0 * 1024.0), 0, 'f', 1);
    }

    QString data_type_str = binaryDataTypeName(_preview_model->parseConfig().data_type);
    QString byte_order_str = _preview_model->parseConfig().byte_order == BinaryByteOrder::LittleEndian
                             ? tr("Little-endian") : tr("Big-endian");

    QString remainder_str;
    if (info.remainder_bytes > 0) {
        remainder_str = tr("<br><b>Warning:</b> %1 remainder bytes at end of data "
                          "(not aligned to element size)")
                       .arg(info.remainder_bytes);
    }

    QString bitwise_str;
    if (_preview_model->parseConfig().bitwise_expand) {
        int bits = binaryDataTypeBits(_preview_model->parseConfig().data_type);
        bitwise_str = tr("<br><b>Bitwise Mode:</b> %1 bits per element").arg(bits);
    }

    QString text = tr(
        "<b>File:</b> %1<br>"
        "<b>File Size:</b> %2<br>"
        "<b>Header Size:</b> %3 bytes<br>"
        "<b>Data Type:</b> %4 (%5 bytes/element)<br>"
        "<b>Byte Order:</b> %6<br>"
        "<b>Channels:</b> %7<br>"
        "<b>Total Elements:</b> %8<br>"
        "<b>Rows:</b> %9<br>"
        "<b>Display Columns:</b> %10"
        "%11%12"
    )
    .arg(fi.fileName())
    .arg(file_size_str)
    .arg(info.data_size > 0 ? QString::number(_preview_model->parseConfig().header_size) : "0")
    .arg(data_type_str)
    .arg(info.item_size)
    .arg(byte_order_str)
    .arg(_preview_model->parseConfig().num_channels)
    .arg(info.total_elements)
    .arg(info.total_rows)
    .arg(info.display_columns)
    .arg(bitwise_str)
    .arg(remainder_str);

    ui->infoLabel->setText(text);
}

void BinaryExplorer_Widget::_populateDataTypeCombo() {
    ui->dataTypeCombo->clear();
    ui->dataTypeCombo->addItem("int8",    static_cast<int>(BinaryDataType::Int8));
    ui->dataTypeCombo->addItem("int16",   static_cast<int>(BinaryDataType::Int16));
    ui->dataTypeCombo->addItem("int32",   static_cast<int>(BinaryDataType::Int32));
    ui->dataTypeCombo->addItem("int64",   static_cast<int>(BinaryDataType::Int64));
    ui->dataTypeCombo->addItem("uint8",   static_cast<int>(BinaryDataType::UInt8));
    ui->dataTypeCombo->addItem("uint16",  static_cast<int>(BinaryDataType::UInt16));
    ui->dataTypeCombo->addItem("uint32",  static_cast<int>(BinaryDataType::UInt32));
    ui->dataTypeCombo->addItem("uint64",  static_cast<int>(BinaryDataType::UInt64));
    ui->dataTypeCombo->addItem("float32", static_cast<int>(BinaryDataType::Float32));
    ui->dataTypeCombo->addItem("float64", static_cast<int>(BinaryDataType::Float64));

    // Default to float32
    ui->dataTypeCombo->setCurrentIndex(8);
}

void BinaryExplorer_Widget::_populateByteOrderCombo() {
    ui->byteOrderCombo->clear();
    ui->byteOrderCombo->addItem("Little-endian", static_cast<int>(BinaryByteOrder::LittleEndian));
    ui->byteOrderCombo->addItem("Big-endian",    static_cast<int>(BinaryByteOrder::BigEndian));
}
