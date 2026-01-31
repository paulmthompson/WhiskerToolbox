#ifndef HDF5_EXPLORER_WIDGET_HPP
#define HDF5_EXPLORER_WIDGET_HPP

/**
 * @file HDF5Explorer_Widget.hpp
 * @brief Widget for browsing and exploring HDF5 file structure
 * 
 * HDF5Explorer_Widget provides a tree-based view of HDF5 files, allowing users
 * to browse groups, datasets, and their attributes. This widget is intended
 * for data exploration and can be integrated with DataImport_Widget.
 * 
 * ## Features
 * 
 * - File selection via dialog or drag-and-drop
 * - Tree view showing HDF5 hierarchy (groups and datasets)
 * - Dataset information panel showing type, dimensions, and attributes
 * - Selection signals for downstream import widgets to use
 * 
 * ## Integration
 * 
 * The widget can be registered with DataImportTypeRegistry as an "HDF5Explorer"
 * type, allowing users to browse HDF5 files before importing specific datasets.
 * 
 * @note This widget is only available when ENABLE_HDF5 is defined.
 */

#include <QWidget>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

// Forward declarations
class DataManager;
class QTreeWidget;
class QTreeWidgetItem;

namespace Ui {
class HDF5Explorer_Widget;
}

/**
 * @brief Information about an HDF5 object (group or dataset)
 */
struct HDF5ObjectInfo {
    QString name;           ///< Object name
    QString full_path;      ///< Full path in the HDF5 file
    bool is_group;          ///< True if group, false if dataset
    QString data_type;      ///< Dataset type (e.g., "float32", "int64")
    QStringList dimensions; ///< Dataset dimensions as strings
    int num_attributes;     ///< Number of attributes
};

/**
 * @brief Widget for browsing HDF5 file structure
 * 
 * Provides a tree view of HDF5 files showing:
 * - Groups (folders)
 * - Datasets with their types and dimensions
 * - Selection of specific datasets for import
 */
class HDF5Explorer_Widget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the HDF5Explorer_Widget
     * 
     * @param data_manager DataManager for future data import integration
     * @param parent Parent widget
     */
    explicit HDF5Explorer_Widget(
        std::shared_ptr<DataManager> data_manager = nullptr,
        QWidget * parent = nullptr);

    ~HDF5Explorer_Widget() override;

    /**
     * @brief Load and display the structure of an HDF5 file
     * @param file_path Path to the HDF5 file
     * @return true if file was loaded successfully
     */
    bool loadFile(QString const & file_path);

    /**
     * @brief Get the currently loaded file path
     * @return File path, or empty if no file loaded
     */
    [[nodiscard]] QString currentFilePath() const { return _current_file_path; }

    /**
     * @brief Get the currently selected dataset path
     * @return Selected dataset path, or empty if no dataset selected
     */
    [[nodiscard]] QString selectedDatasetPath() const;

    /**
     * @brief Get information about the currently selected object
     * @return Object info, or empty struct if nothing selected
     */
    [[nodiscard]] HDF5ObjectInfo selectedObjectInfo() const;

signals:
    /**
     * @brief Emitted when a file is successfully loaded
     * @param file_path Path to the loaded file
     */
    void fileLoaded(QString const & file_path);

    /**
     * @brief Emitted when a dataset is selected in the tree
     * @param dataset_path Full path to the selected dataset
     */
    void datasetSelected(QString const & dataset_path);

    /**
     * @brief Emitted when a dataset is double-clicked (for import)
     * @param dataset_path Full path to the dataset
     * @param info Information about the dataset
     */
    void datasetActivated(QString const & dataset_path, HDF5ObjectInfo const & info);

    /**
     * @brief Emitted when an error occurs
     * @param message Error message
     */
    void errorOccurred(QString const & message);

private slots:
    /**
     * @brief Handle browse button click
     */
    void _onBrowseClicked();

    /**
     * @brief Handle tree item selection change
     */
    void _onTreeSelectionChanged();

    /**
     * @brief Handle tree item double-click
     * @param item The clicked item
     * @param column The clicked column
     */
    void _onTreeItemDoubleClicked(QTreeWidgetItem * item, int column);

    /**
     * @brief Handle refresh button click
     */
    void _onRefreshClicked();

private:
    Ui::HDF5Explorer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    QString _current_file_path;

    /**
     * @brief Clear the tree and info panel
     */
    void _clearDisplay();

    /**
     * @brief Update the info panel with selected object details
     * @param info Object information to display
     */
    void _updateInfoPanel(HDF5ObjectInfo const & info);

    /**
     * @brief Recursively populate the tree with HDF5 file structure
     * 
     * This uses the HDF5 C++ API to traverse the file structure.
     * 
     * @param file_path Path to the HDF5 file
     * @return true if successful
     */
    bool _populateTree(QString const & file_path);

    /**
     * @brief Get info for the currently selected tree item
     * @return Object info from the item's stored data
     */
    [[nodiscard]] HDF5ObjectInfo _getInfoFromItem(QTreeWidgetItem * item) const;
};

#endif // HDF5_EXPLORER_WIDGET_HPP
