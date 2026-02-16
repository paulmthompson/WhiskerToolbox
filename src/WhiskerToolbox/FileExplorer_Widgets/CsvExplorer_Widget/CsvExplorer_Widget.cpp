#include "CsvExplorer_Widget.hpp"
#include "CsvDatasetPreviewModel.hpp"
#include "ui_CsvExplorer_Widget.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>

#include "StateManagement/AppFileDialog.hpp"

CsvExplorer_Widget::CsvExplorer_Widget(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CsvExplorer_Widget),
      _data_manager(std::move(data_manager)),
      _preview_model(new CsvDatasetPreviewModel(this)) {
    ui->setupUi(this);

    // Set up combos
    _populateDelimiterCombo();

    // Set up preview table
    ui->previewTableView->setModel(_preview_model);
    ui->previewTableView->horizontalHeader()->setStretchLastSection(true);
    ui->previewTableView->verticalHeader()->setDefaultSectionSize(24);
    ui->previewTableView->setVisible(false);

    // Connections: file selection
    connect(ui->browseButton, &QPushButton::clicked,
            this, &CsvExplorer_Widget::_onBrowseClicked);
    connect(ui->applyButton, &QPushButton::clicked,
            this, &CsvExplorer_Widget::_onApplyClicked);

    // Connections: config changes trigger live reload
    connect(ui->delimiterCombo, &QComboBox::currentIndexChanged,
            this, &CsvExplorer_Widget::_onConfigChanged);
    connect(ui->customDelimiterEdit, &QLineEdit::textChanged,
            this, &CsvExplorer_Widget::_onConfigChanged);
    connect(ui->headerLinesSpinBox, &QSpinBox::valueChanged,
            this, &CsvExplorer_Widget::_onConfigChanged);
    connect(ui->useHeaderNamesCheckBox, &QCheckBox::toggled,
            this, &CsvExplorer_Widget::_onConfigChanged);
    connect(ui->quoteCharEdit, &QLineEdit::textChanged,
            this, &CsvExplorer_Widget::_onConfigChanged);
    connect(ui->trimWhitespaceCheckBox, &QCheckBox::toggled,
            this, &CsvExplorer_Widget::_onConfigChanged);

    // Connections: model signals
    connect(_preview_model, &CsvDatasetPreviewModel::fileLoaded,
            this, [this](qint64 num_rows, int num_cols) {
                ui->previewStatusLabel->setText(
                    tr("Showing %1 rows x %2 columns (lazy-loaded)")
                    .arg(num_rows).arg(num_cols));
                ui->previewTableView->setVisible(true);
            });
    connect(_preview_model, &CsvDatasetPreviewModel::loadError,
            this, [this](QString const & msg) {
                ui->previewStatusLabel->setText(tr("Error: %1").arg(msg));
                ui->previewTableView->setVisible(false);
            });

    // Show/hide custom delimiter field based on combo selection
    connect(ui->delimiterCombo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                // "Custom" is the last item
                bool is_custom = (index == ui->delimiterCombo->count() - 1);
                ui->customDelimiterEdit->setVisible(is_custom);
                ui->customDelimiterLabel->setVisible(is_custom);
            });

    // Initially hide custom delimiter field
    ui->customDelimiterEdit->setVisible(false);
    ui->customDelimiterLabel->setVisible(false);

    _clearDisplay();
}

CsvExplorer_Widget::~CsvExplorer_Widget() {
    delete ui;
}

bool CsvExplorer_Widget::loadFile(QString const & file_path) {
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

void CsvExplorer_Widget::_onBrowseClicked() {
    QString file_path = AppFileDialog::getOpenFileName(
        this,
        QStringLiteral("import_csv"),
        tr("Select CSV File"),
        tr("CSV Files (*.csv *.tsv *.txt *.dat);;All Files (*)")
    );

    if (!file_path.isEmpty()) {
        loadFile(file_path);
    }
}

void CsvExplorer_Widget::_onApplyClicked() {
    if (!_current_file_path.isEmpty()) {
        loadFile(_current_file_path);
    }
}

void CsvExplorer_Widget::_onConfigChanged() {
    if (_current_file_path.isEmpty() || !_preview_model->hasData()) {
        return;
    }

    auto config = _buildConfigFromUI();
    if (_preview_model->reloadWithConfig(config)) {
        _updateInfoPanel(_preview_model->fileInfo());
    }
}

void CsvExplorer_Widget::_clearDisplay() {
    _preview_model->clear();
    ui->infoLabel->setText(tr("Select a CSV file to view its contents"));
    ui->previewStatusLabel->setText(tr("Select a file to preview its contents"));
    ui->previewTableView->setVisible(false);
}

CsvParseConfig CsvExplorer_Widget::_buildConfigFromUI() const {
    CsvParseConfig config;

    // Get delimiter from combo or custom field
    int delim_index = ui->delimiterCombo->currentIndex();
    bool is_custom = (delim_index == ui->delimiterCombo->count() - 1);

    if (is_custom) {
        QString custom = ui->customDelimiterEdit->text();
        // Interpret escape sequences
        custom.replace("\\t", "\t");
        custom.replace("\\n", "\n");
        custom.replace("\\r", "\r");
        config.column_delimiter = custom.isEmpty() ? "," : custom;
    } else {
        config.column_delimiter = ui->delimiterCombo->currentData().toString();
    }

    config.header_lines = ui->headerLinesSpinBox->value();
    config.use_first_header_as_names = ui->useHeaderNamesCheckBox->isChecked();
    config.quote_char = ui->quoteCharEdit->text();
    config.trim_whitespace = ui->trimWhitespaceCheckBox->isChecked();

    return config;
}

void CsvExplorer_Widget::_updateInfoPanel(CsvFileInfo const & info) {
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

    // Display the delimiter in a readable form
    CsvParseConfig const & cfg = _preview_model->parseConfig();
    QString delim_display = cfg.column_delimiter;
    if (delim_display == "\t") {
        delim_display = "\\t (tab)";
    } else if (delim_display == " ") {
        delim_display = "(space)";
    } else if (delim_display == ",") {
        delim_display = ", (comma)";
    } else if (delim_display == ";") {
        delim_display = "; (semicolon)";
    } else if (delim_display == "|") {
        delim_display = "| (pipe)";
    }

    QString header_info;
    if (cfg.header_lines > 0 && cfg.use_first_header_as_names) {
        header_info = tr("%1 header line(s), using first line as column names")
                      .arg(cfg.header_lines);
    } else if (cfg.header_lines > 0) {
        header_info = tr("%1 header line(s) skipped").arg(cfg.header_lines);
    } else {
        header_info = tr("No header lines");
    }

    QString text = tr(
        "<b>File:</b> %1<br>"
        "<b>File Size:</b> %2<br>"
        "<b>Total Lines:</b> %3<br>"
        "<b>Data Rows:</b> %4<br>"
        "<b>Columns:</b> %5<br>"
        "<b>Delimiter:</b> %6<br>"
        "<b>Header:</b> %7<br>"
        "<b>Quote Character:</b> %8<br>"
        "<b>Trim Whitespace:</b> %9"
    )
    .arg(fi.fileName())
    .arg(file_size_str)
    .arg(info.total_lines)
    .arg(info.data_rows)
    .arg(info.detected_columns)
    .arg(delim_display)
    .arg(header_info)
    .arg(cfg.quote_char.isEmpty() ? tr("(none)") : cfg.quote_char)
    .arg(cfg.trim_whitespace ? tr("Yes") : tr("No"));

    ui->infoLabel->setText(text);
}

void CsvExplorer_Widget::_populateDelimiterCombo() {
    ui->delimiterCombo->clear();
    ui->delimiterCombo->addItem("Comma (,)",      ",");
    ui->delimiterCombo->addItem("Tab (\\t)",       "\t");
    ui->delimiterCombo->addItem("Semicolon (;)",   ";");
    ui->delimiterCombo->addItem("Pipe (|)",        "|");
    ui->delimiterCombo->addItem("Space ( )",       " ");
    ui->delimiterCombo->addItem("Custom...",       "");

    // Default to comma
    ui->delimiterCombo->setCurrentIndex(0);
}
