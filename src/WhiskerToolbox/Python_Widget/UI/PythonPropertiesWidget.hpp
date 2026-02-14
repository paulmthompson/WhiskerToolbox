#ifndef PYTHON_PROPERTIES_WIDGET_HPP
#define PYTHON_PROPERTIES_WIDGET_HPP

/**
 * @file PythonPropertiesWidget.hpp
 * @brief Properties panel for the Python Widget
 *
 * Provides:
 * - Environment info (Python version, working directory)
 * - Virtual environment selector (Phase 6)
 * - Settings (font size, auto-scroll, line numbers)
 * - Script arguments (sys.argv)
 * - Auto-import prelude (configurable startup code)
 * - Namespace inspector (table of current Python variables)
 * - DataManager keys list with "Insert" button
 *
 * @see PythonWidgetState for the shared state class
 * @see PythonConsoleWidget for the main console widget
 */

#include <QWidget>

#include <memory>

class DataManager;
class PythonBridge;
class PythonWidgetState;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

class PythonPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    explicit PythonPropertiesWidget(std::shared_ptr<PythonWidgetState> state,
                                    PythonBridge * bridge,
                                    std::shared_ptr<DataManager> data_manager,
                                    QWidget * parent = nullptr);
    ~PythonPropertiesWidget() override;

    /// Refresh the namespace inspector and data keys list
    void refreshNamespace();

signals:
    /// Emitted when user clicks "Insert" on a data key
    void insertCodeRequested(QString const & code);

private:
    void _setupUI();
    void _refreshDataKeys();
    void _onFontSizeChanged(int size);
    void _onAutoScrollChanged(bool enabled);
    void _onShowLineNumbersChanged(bool enabled);
    void _onClearNamespace();
    void _onBrowseWorkingDirectory();
    void _onApplyWorkingDirectory();
    void _onScriptArgumentsChanged();
    void _onPreludeEnabledChanged(bool enabled);
    void _onApplyPrelude();
    void _onVenvSelected(int index);
    void _onBrowseVenv();
    void _onRefreshVenvs();
    void _onDeactivateVenv();
    void _onRefreshPackages();
    void _onInstallPackage();

    void _populateVenvCombo();
    void _updateVenvIndicator();

    std::shared_ptr<PythonWidgetState> _state;
    PythonBridge * _bridge;
    std::shared_ptr<DataManager> _data_manager;

    // Environment section
    QLabel * _python_version_label{nullptr};
    QLineEdit * _working_dir_edit{nullptr};

    // Virtual environment section (Phase 6)
    QComboBox * _venv_combo{nullptr};
    QLabel * _venv_status_label{nullptr};
    QTreeWidget * _packages_tree{nullptr};
    QLineEdit * _install_package_edit{nullptr};

    // Settings
    QSpinBox * _font_size_spin{nullptr};
    QCheckBox * _auto_scroll_check{nullptr};
    QCheckBox * _line_numbers_check{nullptr};
    QPushButton * _clear_namespace_button{nullptr};

    // Script arguments
    QLineEdit * _script_args_edit{nullptr};

    // Auto-import prelude
    QCheckBox * _prelude_enabled_check{nullptr};
    QPlainTextEdit * _prelude_edit{nullptr};

    // Namespace inspector
    QTreeWidget * _namespace_tree{nullptr};

    // Data keys
    QTreeWidget * _data_keys_tree{nullptr};
};

#endif // PYTHON_PROPERTIES_WIDGET_HPP
