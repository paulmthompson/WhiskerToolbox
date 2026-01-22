#ifndef ZONE_MANAGER_WIDGET_HPP
#define ZONE_MANAGER_WIDGET_HPP

/**
 * @file ZoneManagerWidget.hpp
 * @brief Widget for viewing and editing zone layout configuration
 * 
 * ZoneManagerWidget provides a UI for:
 * - Loading zone configuration from JSON files
 * - Saving current layout to JSON files
 * - Viewing and editing zone ratios
 * - Enabling/disabling auto-save
 * 
 * ## Usage
 * 
 * ```cpp
 * auto * widget = new ZoneManagerWidget(zone_manager);
 * // Add to your layout or dock
 * ```
 * 
 * @see ZoneManager for the underlying zone management
 * @see ZoneConfig for the configuration data structures
 */

#include <QWidget>

#include <memory>

// Forward declarations
class QLabel;
class QPushButton;
class QDoubleSpinBox;
class QCheckBox;
class QLineEdit;
class QGroupBox;
class QTextEdit;
class ZoneManager;

/**
 * @brief Widget for managing zone layout configuration
 */
class ZoneManagerWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a ZoneManagerWidget
     * @param zone_manager The ZoneManager to configure (must not be null)
     * @param parent Parent widget
     */
    explicit ZoneManagerWidget(ZoneManager * zone_manager, QWidget * parent = nullptr);

    ~ZoneManagerWidget() override = default;

signals:
    /**
     * @brief Emitted when configuration is successfully loaded
     * @param file_path Path to the loaded file
     */
    void configurationLoaded(QString const & file_path);

    /**
     * @brief Emitted when configuration is successfully saved
     * @param file_path Path to the saved file
     */
    void configurationSaved(QString const & file_path);

private slots:
    void onLoadConfigClicked();
    void onSaveConfigClicked();
    void onApplyRatiosClicked();
    void onAutoSaveToggled(bool enabled);
    void onAutoSavePathChanged();
    void onZoneRatiosChanged();
    void onConfigLoaded(QString const & file_path);
    void onConfigSaved(QString const & file_path);
    void onConfigLoadError(QString const & error);

private:
    void setupUi();
    void connectSignals();
    void updateRatioDisplay();
    void logMessage(QString const & message);

    ZoneManager * _zone_manager;

    // UI Elements - File Operations
    QPushButton * _load_config_btn;
    QPushButton * _save_config_btn;
    QLineEdit * _config_path_edit;

    // UI Elements - Zone Ratios
    QGroupBox * _ratios_group;
    QDoubleSpinBox * _left_ratio_spin;
    QDoubleSpinBox * _center_ratio_spin;
    QDoubleSpinBox * _right_ratio_spin;
    QDoubleSpinBox * _bottom_ratio_spin;
    QPushButton * _apply_ratios_btn;

    // UI Elements - Auto-save
    QGroupBox * _autosave_group;
    QCheckBox * _autosave_enabled_check;
    QLineEdit * _autosave_path_edit;
    QPushButton * _browse_autosave_btn;

    // UI Elements - Status
    QTextEdit * _log_text;
};

#endif  // ZONE_MANAGER_WIDGET_HPP
