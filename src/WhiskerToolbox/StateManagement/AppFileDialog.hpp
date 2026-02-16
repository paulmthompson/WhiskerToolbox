#ifndef APP_FILE_DIALOG_HPP
#define APP_FILE_DIALOG_HPP

/**
 * @file AppFileDialog.hpp
 * @brief Drop-in replacement for QFileDialog static methods with automatic path memory
 *
 * AppFileDialog wraps QFileDialog::getOpenFileName, getSaveFileName, and
 * getExistingDirectory with automatic session-based path memory. Widgets
 * switch from QFileDialog to AppFileDialog by changing one function call:
 *
 * ```cpp
 * // Before:
 * auto path = QFileDialog::getOpenFileName(this, "Open CSV", "", "CSV (*.csv)");
 *
 * // After:
 * auto path = AppFileDialog::getOpenFileName(this, "import_csv", "Open CSV", "CSV (*.csv)");
 * ```
 *
 * The dialog_id groups related dialogs so they share path memory. Suggested IDs:
 * - Import: "import_csv", "import_hdf5", "import_hdf5_dir", "import_binary",
 *           "import_numpy", "import_mask_dir", "import_whisker", "import_model_weights"
 * - Export: "export_svg", "export_csv", "export_dir", "export_mask_dir"
 * - Python: "python_script", "python_working_dir", "python_venv"
 * - Config: "table_json", "zone_config", "spike_sorter_config",
 *           "batch_folder", "batch_json", "transform_json"
 *
 * Call AppFileDialog::init() once at startup (from MainWindow) to wire in the SessionStore.
 * If init() was never called, the wrappers fall through to plain QFileDialog behavior.
 *
 * @see SessionStore for the underlying path memory
 * @see STATE_MANAGEMENT_ROADMAP.md Phase 1
 */

#include <QFileDialog>
#include <QString>

namespace StateManagement {
class SessionStore;
}

namespace AppFileDialog {

/**
 * @brief Initialize with a SessionStore for path memory
 *
 * Must be called once before any dialog wrappers are used. If never called,
 * the wrappers behave identically to plain QFileDialog (no path memory).
 *
 * @param session Pointer to the SessionStore (must outlive all dialog calls)
 */
void init(StateManagement::SessionStore * session);

/**
 * @brief Wrap QFileDialog::getOpenFileName with automatic path memory
 *
 * @param parent Parent widget
 * @param dialog_id Identifies this dialog category for path memory (e.g., "import_csv")
 * @param caption Dialog window title
 * @param filter File type filter string
 * @param fallback_dir Directory to use if no session path exists (default: cwd)
 * @return Selected file path, or empty string if cancelled
 */
[[nodiscard]] QString getOpenFileName(
        QWidget * parent,
        QString const & dialog_id,
        QString const & caption = {},
        QString const & filter = {},
        QString const & fallback_dir = {});

/**
 * @brief Wrap QFileDialog::getSaveFileName with automatic path memory
 *
 * @param parent Parent widget
 * @param dialog_id Identifies this dialog category for path memory (e.g., "export_csv")
 * @param caption Dialog window title
 * @param filter File type filter string
 * @param fallback_dir Directory to use if no session path exists (default: cwd)
 * @param suggested_name Optional suggested filename (appended to the directory)
 * @return Selected file path, or empty string if cancelled
 */
[[nodiscard]] QString getSaveFileName(
        QWidget * parent,
        QString const & dialog_id,
        QString const & caption = {},
        QString const & filter = {},
        QString const & fallback_dir = {},
        QString const & suggested_name = {});

/**
 * @brief Wrap QFileDialog::getExistingDirectory with automatic path memory
 *
 * @param parent Parent widget
 * @param dialog_id Identifies this dialog category for path memory (e.g., "import_hdf5_dir")
 * @param caption Dialog window title
 * @param fallback_dir Directory to use if no session path exists (default: cwd)
 * @param options QFileDialog options (default: ShowDirsOnly)
 * @return Selected directory path, or empty string if cancelled
 */
[[nodiscard]] QString getExistingDirectory(
        QWidget * parent,
        QString const & dialog_id,
        QString const & caption = {},
        QString const & fallback_dir = {},
        QFileDialog::Options options = QFileDialog::ShowDirsOnly);

}// namespace AppFileDialog

#endif// APP_FILE_DIALOG_HPP
