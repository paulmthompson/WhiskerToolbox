/**
 * @file PipelineLibraryDialog.cpp
 * @brief Implementation of the TransformsV2 pipeline library browser dialog
 */

#include "PipelineLibraryDialog.hpp"
#include "ui_PipelineLibraryDialog.h"

#include "StateManagement/AppFileDialog.hpp"
#include "TransformsV2/io/PipelineLibrary.hpp"

#include <QDesktopServices>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QTextEdit>
#include <QUrl>

using namespace Neuralyzer::Transforms::V2::Examples;

PipelineLibraryDialog::PipelineLibraryDialog(QString const & library_dir, QWidget * parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::PipelineLibraryDialog>()),
      _library_dir(library_dir.toStdString()) {
    ui->setupUi(this);
    setWindowTitle(tr("Pipeline Library"));
    ui->libraryPathLabel->setText(
            tr("Library folder: %1").arg(QString::fromStdString(_library_dir.string())));

    refreshCatalog();
    _setupConnections();
    _updateActionButtons();
}

PipelineLibraryDialog::~PipelineLibraryDialog() = default;

void PipelineLibraryDialog::setDescriptorSupplier(DescriptorSupplier supplier) {
    _descriptor_supplier = std::move(supplier);
}

void PipelineLibraryDialog::refreshCatalog() {
    _entries = listPipelineFiles(_library_dir);

    ui->pipelineListWidget->clear();
    for (auto const & entry: _entries) {
        auto * item = new QListWidgetItem(QString::fromStdString(entry.display_name));
        item->setData(Qt::UserRole, QString::fromStdString(entry.id));
        item->setToolTip(QString::fromStdString(entry.path.string()));
        ui->pipelineListWidget->addItem(item);
    }

    _clearPreview();
    _updateActionButtons();
}

std::string PipelineLibraryDialog::loadedPipelineJson() const {
    return _loaded_pipeline_json.value_or(std::string{});
}

int PipelineLibraryDialog::catalogEntryCount() const {
    return static_cast<int>(_entries.size());
}

void PipelineLibraryDialog::_setupConnections() {
    connect(ui->pipelineListWidget, &QListWidget::currentRowChanged,
            this, [this](int /*row*/) { _onSelectionChanged(); });

    connect(ui->loadButton, &QPushButton::clicked, this, &PipelineLibraryDialog::_onLoadClicked);
    connect(ui->saveToLibraryButton, &QPushButton::clicked,
            this, &PipelineLibraryDialog::_onSaveToLibraryClicked);
    connect(ui->deleteButton, &QPushButton::clicked, this, &PipelineLibraryDialog::_onDeleteClicked);
    connect(ui->importButton, &QPushButton::clicked, this, &PipelineLibraryDialog::_onImportClicked);
    connect(ui->exportButton, &QPushButton::clicked, this, &PipelineLibraryDialog::_onExportClicked);
    connect(ui->openFolderButton, &QPushButton::clicked,
            this, &PipelineLibraryDialog::_onOpenFolderClicked);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void PipelineLibraryDialog::_updateActionButtons() {
    auto const has_selection = _selectedEntry().has_value();
    ui->loadButton->setEnabled(has_selection);
    ui->deleteButton->setEnabled(has_selection);
    ui->exportButton->setEnabled(has_selection);
    ui->saveToLibraryButton->setEnabled(static_cast<bool>(_descriptor_supplier));
}

void PipelineLibraryDialog::_onSelectionChanged() {
    if (auto const entry = _selectedEntry()) {
        _setPreviewForEntry(*entry);
    } else {
        _clearPreview();
    }
    _updateActionButtons();
}

std::optional<PipelineLibraryEntry> PipelineLibraryDialog::_selectedEntry() const {
    auto const row = ui->pipelineListWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(_entries.size())) {
        return std::nullopt;
    }
    return _entries[static_cast<size_t>(row)];
}

void PipelineLibraryDialog::_setPreviewForEntry(PipelineLibraryEntry const & entry) {
    auto const load_result = loadPipelineDescriptorFromFile(entry.path);
    if (!load_result) {
        ui->previewTextEdit->setPlainText(
                tr("Failed to load preview:\n%1")
                        .arg(QString::fromStdString(load_result.error()->what())));
        return;
    }

    ui->previewTextEdit->setPlainText(
            QString::fromStdString(savePipelineToJson(load_result.value())));
}

void PipelineLibraryDialog::_clearPreview() {
    ui->previewTextEdit->clear();
}

void PipelineLibraryDialog::_onLoadClicked() {
    auto const entry = _selectedEntry();
    if (!entry) {
        return;
    }

    auto const load_result = loadPipelineDescriptorFromFile(entry->path);
    if (!load_result) {
        QMessageBox::warning(this, tr("Load Failed"),
                             tr("Could not load pipeline:\n%1")
                                     .arg(QString::fromStdString(load_result.error()->what())));
        return;
    }

    _loaded_pipeline_json = savePipelineToJson(load_result.value());
    accept();
}

void PipelineLibraryDialog::_onSaveToLibraryClicked() {
    if (!_descriptor_supplier) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("No pipeline is available to save."));
        return;
    }

    auto descriptor = _descriptor_supplier();

    auto const default_name =
            descriptor.metadata.has_value() && descriptor.metadata->name.has_value()
                    ? QString::fromStdString(*descriptor.metadata->name)
                    : tr("Untitled Pipeline");

    bool ok = false;
    auto const name = QInputDialog::getText(
            this, tr("Save to Library"), tr("Pipeline name:"), QLineEdit::Normal, default_name, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    if (!descriptor.metadata.has_value()) {
        descriptor.metadata = PipelineMetadata{};
    }
    descriptor.metadata->name = name.trimmed().toStdString();

    auto const filename = sanitizePipelineFilename(descriptor.metadata->name.value()) + ".json";
    auto const dest_path = _library_dir / filename;

    if (std::filesystem::exists(dest_path)) {
        auto const overwrite = QMessageBox::question(
                this,
                tr("Overwrite Pipeline"),
                tr("A pipeline named '%1' already exists. Overwrite?").arg(
                        QString::fromStdString(filename)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        if (overwrite != QMessageBox::Yes) {
            return;
        }
    }

    auto const save_result = savePipelineDescriptorToFile(dest_path, descriptor);
    if (!save_result) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not save pipeline:\n%1")
                                     .arg(QString::fromStdString(save_result.error()->what())));
        return;
    }

    refreshCatalog();

    auto const stem = dest_path.stem().string();
    for (int row = 0; row < ui->pipelineListWidget->count(); ++row) {
        if (ui->pipelineListWidget->item(row)->data(Qt::UserRole).toString().toStdString() == stem) {
            ui->pipelineListWidget->setCurrentRow(row);
            break;
        }
    }
}

void PipelineLibraryDialog::_onDeleteClicked() {
    auto const entry = _selectedEntry();
    if (!entry) {
        return;
    }

    auto const confirm = QMessageBox::question(
            this,
            tr("Delete Pipeline"),
            tr("Delete '%1' from the library? This cannot be undone.")
                    .arg(QString::fromStdString(entry->display_name)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
    if (confirm != QMessageBox::Yes) {
        return;
    }

    auto const delete_result = deletePipelineFile(entry->path);
    if (!delete_result) {
        QMessageBox::warning(this, tr("Delete Failed"),
                             tr("Could not delete pipeline:\n%1")
                                     .arg(QString::fromStdString(delete_result.error()->what())));
        return;
    }

    refreshCatalog();
}

void PipelineLibraryDialog::_onImportClicked() {
    auto const filepath = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("transformv2_pipeline_open"),
            tr("Import Pipeline JSON"),
            tr("JSON Files (*.json);;All Files (*)"),
            QString::fromStdString(_library_dir.string()));

    if (filepath.isEmpty()) {
        return;
    }

    auto const load_result = loadPipelineDescriptorFromFile(filepath.toStdString());
    if (!load_result) {
        QMessageBox::warning(this, tr("Import Failed"),
                             tr("Could not read pipeline:\n%1")
                                     .arg(QString::fromStdString(load_result.error()->what())));
        return;
    }

    auto descriptor = load_result.value();
    auto const stem = std::filesystem::path(filepath.toStdString()).stem().string();
    if (!descriptor.metadata.has_value()) {
        descriptor.metadata = PipelineMetadata{};
    }
    if (!descriptor.metadata->name.has_value() || descriptor.metadata->name->empty()) {
        descriptor.metadata->name = stem;
    }

    auto const filename = sanitizePipelineFilename(*descriptor.metadata->name) + ".json";
    auto const dest_path = _library_dir / filename;

    if (std::filesystem::exists(dest_path)) {
        auto const overwrite = QMessageBox::question(
                this,
                tr("Overwrite Pipeline"),
                tr("'%1' already exists in the library. Overwrite?").arg(
                        QString::fromStdString(filename)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
        if (overwrite != QMessageBox::Yes) {
            return;
        }
    }

    auto const save_result = savePipelineDescriptorToFile(dest_path, descriptor);
    if (!save_result) {
        QMessageBox::warning(this, tr("Import Failed"),
                             tr("Could not import pipeline:\n%1")
                                     .arg(QString::fromStdString(save_result.error()->what())));
        return;
    }

    refreshCatalog();
}

void PipelineLibraryDialog::_onExportClicked() {
    auto const entry = _selectedEntry();
    if (!entry) {
        return;
    }

    auto const suggested = QString::fromStdString(entry->path.filename().string());
    auto const dest_path = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("transformv2_pipeline_save"),
            tr("Export Pipeline JSON"),
            tr("JSON Files (*.json);;All Files (*)"),
            QString::fromStdString(_library_dir.string()),
            suggested);

    if (dest_path.isEmpty()) {
        return;
    }

    std::error_code error;
    std::filesystem::copy_file(entry->path, dest_path.toStdString(), std::filesystem::copy_options::overwrite_existing, error);
    if (error) {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not export pipeline:\n%1")
                                     .arg(QString::fromStdString(error.message())));
    }
}

void PipelineLibraryDialog::_onOpenFolderClicked() {
    std::error_code error;
    std::filesystem::create_directories(_library_dir, error);

    auto const url = QUrl::fromLocalFile(QString::fromStdString(_library_dir.string()));
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, tr("Open Folder Failed"),
                             tr("Could not open the library folder in the file manager."));
    }
}
