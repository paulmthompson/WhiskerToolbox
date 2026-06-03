#ifndef PIPELINE_LIBRARY_DIALOG_HPP
#define PIPELINE_LIBRARY_DIALOG_HPP

/**
 * @file PipelineLibraryDialog.hpp
 * @brief Dialog for browsing and managing saved TransformsV2 PipelineDescriptor files
 */

#include "TransformsV2/io/PipelineLoader.hpp"

#include <QDialog>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Ui {
class PipelineLibraryDialog;
}

namespace WhiskerToolbox::Transforms::V2::Examples {
struct PipelineLibraryEntry;
}// namespace WhiskerToolbox::Transforms::V2::Examples

/**
 * @brief Browse, load, save, import, export, and delete library pipeline JSON files
 */
class PipelineLibraryDialog : public QDialog {
    Q_OBJECT

public:
    using DescriptorSupplier = std::function<WhiskerToolbox::Transforms::V2::Examples::PipelineDescriptor()>;

    /**
     * @brief Construct the library dialog
     * @param library_dir Directory containing saved pipeline JSON files
     * @param parent Parent widget
     */
    explicit PipelineLibraryDialog(QString const & library_dir, QWidget * parent = nullptr);
    ~PipelineLibraryDialog() override;

    /**
     * @brief Supply the current pipeline when saving from this dialog
     * @param supplier Callable returning the descriptor to persist
     */
    void setDescriptorSupplier(DescriptorSupplier supplier);

    /**
     * @brief Refresh the pipeline list from disk
     */
    void refreshCatalog();

    /**
     * @brief @return true if the user loaded a pipeline and closed with Accept
     */
    [[nodiscard]] bool hasLoadedPipeline() const { return _loaded_pipeline_json.has_value(); }

    /**
     * @brief JSON text of the pipeline loaded by the user
     * @pre hasLoadedPipeline()
     */
    [[nodiscard]] std::string loadedPipelineJson() const;

    /**
     * @brief Number of entries currently shown (for tests)
     */
    [[nodiscard]] int catalogEntryCount() const;

private slots:
    void _onSelectionChanged();
    void _onLoadClicked();
    void _onSaveToLibraryClicked();
    void _onDeleteClicked();
    void _onImportClicked();
    void _onExportClicked();
    void _onOpenFolderClicked();

private:
    void _setupConnections();
    void _updateActionButtons();
    [[nodiscard]] std::optional<WhiskerToolbox::Transforms::V2::Examples::PipelineLibraryEntry>
    _selectedEntry() const;
    void _setPreviewForEntry(
            WhiskerToolbox::Transforms::V2::Examples::PipelineLibraryEntry const & entry);
    void _clearPreview();

    std::unique_ptr<Ui::PipelineLibraryDialog> ui;
    std::filesystem::path _library_dir;
    std::vector<WhiskerToolbox::Transforms::V2::Examples::PipelineLibraryEntry> _entries;
    DescriptorSupplier _descriptor_supplier;
    std::optional<std::string> _loaded_pipeline_json;
};

#endif// PIPELINE_LIBRARY_DIALOG_HPP
