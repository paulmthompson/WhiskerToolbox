#ifndef DATA_IMPORT_TYPE_REGISTRY_HPP
#define DATA_IMPORT_TYPE_REGISTRY_HPP

/**
 * @file DataImportTypeRegistry.hpp
 * @brief Registry mapping data types to loader widget factories
 * 
 * DataImportTypeRegistry provides a central registry for all data type import widgets.
 * Each data type (LineData, MaskData, PointData, etc.) can register a factory function
 * that creates the appropriate loader widget.
 * 
 * ## Registration Pattern
 * 
 * Format-specific loader widgets register themselves at static initialization time:
 * 
 * ```cpp
 * // In LineImport_Widget.cpp
 * namespace {
 * struct LineImportRegistrar {
 *     LineImportRegistrar() {
 *         DataImportTypeRegistry::instance().registerType(
 *             "LineData",
 *             ImportWidgetFactory{
 *                 .display_name = "Line Data",
 *                 .create_widget = [](auto dm, auto parent) {
 *                     return new LineImport_Widget(dm, parent);
 *                 }
 *             });
 *     }
 * } line_import_registrar;
 * }
 * ```
 * 
 * ## Usage in DataImport_Widget
 * 
 * ```cpp
 * void DataImport_Widget::onDataFocusChanged(QString const& data_type) {
 *     auto& registry = DataImportTypeRegistry::instance();
 *     if (registry.hasType(data_type)) {
 *         QWidget* widget = _type_widgets[data_type];
 *         if (!widget) {
 *             widget = registry.createWidget(data_type, _data_manager, this);
 *             _type_widgets[data_type] = widget;
 *             _stacked_widget->addWidget(widget);
 *         }
 *         _stacked_widget->setCurrentWidget(widget);
 *     }
 * }
 * ```
 * 
 * @see DataImport_Widget for the main container widget
 * @see DataImportWidgetState for state management
 */

#include <QWidget>
#include <QString>
#include <QStringList>

#include <functional>
#include <map>
#include <memory>

class DataManager;

/**
 * @brief Factory structure for creating import widgets
 * 
 * Contains the metadata and factory function for a data type's import widget.
 */
struct ImportWidgetFactory {
    QString display_name;  ///< User-visible name (e.g., "Line Data")
    
    /**
     * @brief Factory function to create the import widget
     * 
     * @param data_manager Shared pointer to DataManager for data access
     * @param parent Parent widget (for Qt ownership)
     * @return Newly created import widget
     */
    std::function<QWidget*(std::shared_ptr<DataManager>, QWidget*)> create_widget;
};

/**
 * @brief Singleton registry mapping data types to import widget factories
 * 
 * Thread safety: Registration should happen at static initialization time.
 * Widget creation happens on the main thread.
 */
class DataImportTypeRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry
     */
    static DataImportTypeRegistry & instance();

    // Non-copyable, non-movable singleton
    DataImportTypeRegistry(DataImportTypeRegistry const &) = delete;
    DataImportTypeRegistry & operator=(DataImportTypeRegistry const &) = delete;
    DataImportTypeRegistry(DataImportTypeRegistry &&) = delete;
    DataImportTypeRegistry & operator=(DataImportTypeRegistry &&) = delete;

    /**
     * @brief Register an import widget factory for a data type
     * 
     * @param data_type Data type identifier (e.g., "LineData", "MaskData")
     * @param factory Factory structure with display name and creation function
     */
    void registerType(QString const & data_type, ImportWidgetFactory factory);

    /**
     * @brief Check if a data type has a registered import widget
     * @param data_type Data type to check
     * @return true if registered
     */
    [[nodiscard]] bool hasType(QString const & data_type) const;

    /**
     * @brief Create an import widget for a data type
     * 
     * @param data_type Data type to create widget for
     * @param dm DataManager for data access
     * @param parent Parent widget for Qt ownership
     * @return Newly created widget, or nullptr if type not registered
     */
    [[nodiscard]] QWidget * createWidget(QString const & data_type,
                                          std::shared_ptr<DataManager> dm,
                                          QWidget * parent = nullptr) const;

    /**
     * @brief Get list of all registered data types
     * @return List of registered type strings
     */
    [[nodiscard]] QStringList supportedTypes() const;

    /**
     * @brief Get display name for a data type
     * @param data_type Data type to look up
     * @return Display name, or empty string if not registered
     */
    [[nodiscard]] QString displayName(QString const & data_type) const;

private:
    DataImportTypeRegistry() = default;
    ~DataImportTypeRegistry() = default;

    std::map<QString, ImportWidgetFactory> _factories;
};

#endif // DATA_IMPORT_TYPE_REGISTRY_HPP
