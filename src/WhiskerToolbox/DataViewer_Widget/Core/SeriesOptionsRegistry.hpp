#ifndef SERIES_OPTIONS_REGISTRY_HPP
#define SERIES_OPTIONS_REGISTRY_HPP

/**
 * @file SeriesOptionsRegistry.hpp
 * @brief Generic type-safe registry for series display options
 * 
 * This class provides a unified API for managing all series display option types
 * (Analog, DigitalEvent, DigitalInterval) instead of having separate methods for
 * each type in DataViewerState.
 * 
 * ## Design Goals
 * 
 * 1. **Type safety**: Template methods ensure compile-time type checking
 * 2. **Single API**: One set of methods (set, get, remove, keys) for all types
 * 3. **Signal consolidation**: Three signals cover all change types
 * 4. **Non-owning**: Points to DataViewerStateData owned by DataViewerState
 * 
 * ## Usage
 * 
 * ```cpp
 * // Through DataViewerState
 * auto& registry = state->seriesOptions();
 * 
 * // Set options (type inferred from argument)
 * AnalogSeriesOptionsData opts;
 * opts.hex_color() = "#ff0000";
 * opts.user_scale_factor = 2.0f;
 * registry.set("channel_1", opts);
 * 
 * // Get options (type specified explicitly)
 * auto const* analog = registry.get<AnalogSeriesOptionsData>("channel_1");
 * 
 * // Check existence
 * if (registry.has<AnalogSeriesOptionsData>("channel_1")) { ... }
 * 
 * // Get all keys for a type
 * QStringList keys = registry.keys<AnalogSeriesOptionsData>();
 * 
 * // Get only visible keys
 * QStringList visible = registry.visibleKeys<AnalogSeriesOptionsData>();
 * ```
 * 
 * @see DataViewerState
 * @see DataViewerStateData
 */

#include "DataViewerStateData.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Generic registry for all series display option types
 * 
 * Provides a unified API for managing AnalogSeriesOptionsData,
 * DigitalEventSeriesOptionsData, and DigitalIntervalSeriesOptionsData.
 */
class SeriesOptionsRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a SeriesOptionsRegistry
     * @param data Non-owning pointer to the underlying state data
     * @param parent Parent QObject (typically DataViewerState)
     */
    explicit SeriesOptionsRegistry(DataViewerStateData * data, QObject * parent = nullptr);

    ~SeriesOptionsRegistry() override = default;

    // === Generic Type-Safe API ===

    /**
     * @brief Set display options for a key
     * 
     * The type is inferred from the options argument.
     * Emits optionsChanged signal.
     * 
     * @tparam T Display options type (AnalogSeriesOptionsData, etc.)
     * @param key The data key (e.g., "channel_1")
     * @param options The display options to store
     */
    template<typename T>
    void set(QString const & key, T const & options);

    /**
     * @brief Get display options for a key
     * 
     * @tparam T Display options type to retrieve
     * @param key The data key
     * @return Pointer to options, or nullptr if not found
     */
    template<typename T>
    [[nodiscard]] T const * get(QString const & key) const;

    /**
     * @brief Get mutable display options for a key
     * 
     * Use sparingly - prefer set() to ensure signals are emitted.
     * Call notifyChanged() after modifying the returned options.
     * 
     * @tparam T Display options type to retrieve
     * @param key The data key
     * @return Pointer to options, or nullptr if not found
     */
    template<typename T>
    [[nodiscard]] T * getMutable(QString const & key);

    /**
     * @brief Remove display options for a key
     * 
     * Emits optionsRemoved signal if options existed.
     * 
     * @tparam T Display options type to remove
     * @param key The data key
     * @return true if options were found and removed
     */
    template<typename T>
    bool remove(QString const & key);

    /**
     * @brief Check if options exist for a key
     * 
     * @tparam T Display options type to check
     * @param key The data key
     * @return true if options exist
     */
    template<typename T>
    [[nodiscard]] bool has(QString const & key) const;

    /**
     * @brief Get all keys that have options of a given type
     * 
     * @tparam T Display options type
     * @return List of keys
     */
    template<typename T>
    [[nodiscard]] QStringList keys() const;

    /**
     * @brief Get keys where options have is_visible == true
     * 
     * @tparam T Display options type
     * @return List of visible keys
     */
    template<typename T>
    [[nodiscard]] QStringList visibleKeys() const;

    /**
     * @brief Get count of options for a given type
     * 
     * @tparam T Display options type
     * @return Number of stored options
     */
    template<typename T>
    [[nodiscard]] int count() const;

    // === Visibility Convenience Methods ===

    /**
     * @brief Set visibility for options by key and type name
     * 
     * @param key The data key
     * @param type_name Type name ("analog", "event", "interval")
     * @param visible New visibility state
     * @return true if options were found and updated
     */
    bool setVisible(QString const & key, QString const & type_name, bool visible);

    /**
     * @brief Get visibility for options by key and type name
     * 
     * @param key The data key
     * @param type_name Type name
     * @return true if options exist and are visible
     */
    [[nodiscard]] bool isVisible(QString const & key, QString const & type_name) const;

    // === Type Name Utilities ===

    /**
     * @brief Get the type name string for a display options type
     * 
     * @tparam T Display options type
     * @return Type name ("analog", "event", "interval")
     */
    template<typename T>
    [[nodiscard]] static QString typeName();

    // === Notification ===

    /**
     * @brief Manually emit optionsChanged signal
     * 
     * Call this after using getMutable() to modify options in-place.
     * 
     * @tparam T Display options type that was modified
     * @param key The data key
     */
    template<typename T>
    void notifyChanged(QString const & key);

signals:
    /**
     * @brief Emitted when display options are set or modified
     * @param key The data key
     * @param type_name The type name ("analog", "event", "interval")
     */
    void optionsChanged(QString const & key, QString const & type_name);

    /**
     * @brief Emitted when display options are removed
     * @param key The data key
     * @param type_name The type name
     */
    void optionsRemoved(QString const & key, QString const & type_name);

    /**
     * @brief Emitted when visibility changes
     * @param key The data key
     * @param type_name The type name
     * @param visible New visibility state
     */
    void visibilityChanged(QString const & key, QString const & type_name, bool visible);

private:
    DataViewerStateData * _data;  // Non-owning pointer
};

// === Template Specialization Declarations ===

// Type name specializations
template<> QString SeriesOptionsRegistry::typeName<AnalogSeriesOptionsData>();
template<> QString SeriesOptionsRegistry::typeName<DigitalEventSeriesOptionsData>();
template<> QString SeriesOptionsRegistry::typeName<DigitalIntervalSeriesOptionsData>();

// set() specializations
template<> void SeriesOptionsRegistry::set<AnalogSeriesOptionsData>(QString const & key, AnalogSeriesOptionsData const & options);
template<> void SeriesOptionsRegistry::set<DigitalEventSeriesOptionsData>(QString const & key, DigitalEventSeriesOptionsData const & options);
template<> void SeriesOptionsRegistry::set<DigitalIntervalSeriesOptionsData>(QString const & key, DigitalIntervalSeriesOptionsData const & options);

// get() specializations
template<> AnalogSeriesOptionsData const * SeriesOptionsRegistry::get<AnalogSeriesOptionsData>(QString const & key) const;
template<> DigitalEventSeriesOptionsData const * SeriesOptionsRegistry::get<DigitalEventSeriesOptionsData>(QString const & key) const;
template<> DigitalIntervalSeriesOptionsData const * SeriesOptionsRegistry::get<DigitalIntervalSeriesOptionsData>(QString const & key) const;

// getMutable() specializations
template<> AnalogSeriesOptionsData * SeriesOptionsRegistry::getMutable<AnalogSeriesOptionsData>(QString const & key);
template<> DigitalEventSeriesOptionsData * SeriesOptionsRegistry::getMutable<DigitalEventSeriesOptionsData>(QString const & key);
template<> DigitalIntervalSeriesOptionsData * SeriesOptionsRegistry::getMutable<DigitalIntervalSeriesOptionsData>(QString const & key);

// remove() specializations
template<> bool SeriesOptionsRegistry::remove<AnalogSeriesOptionsData>(QString const & key);
template<> bool SeriesOptionsRegistry::remove<DigitalEventSeriesOptionsData>(QString const & key);
template<> bool SeriesOptionsRegistry::remove<DigitalIntervalSeriesOptionsData>(QString const & key);

// has() specializations
template<> bool SeriesOptionsRegistry::has<AnalogSeriesOptionsData>(QString const & key) const;
template<> bool SeriesOptionsRegistry::has<DigitalEventSeriesOptionsData>(QString const & key) const;
template<> bool SeriesOptionsRegistry::has<DigitalIntervalSeriesOptionsData>(QString const & key) const;

// keys() specializations
template<> QStringList SeriesOptionsRegistry::keys<AnalogSeriesOptionsData>() const;
template<> QStringList SeriesOptionsRegistry::keys<DigitalEventSeriesOptionsData>() const;
template<> QStringList SeriesOptionsRegistry::keys<DigitalIntervalSeriesOptionsData>() const;

// visibleKeys() specializations
template<> QStringList SeriesOptionsRegistry::visibleKeys<AnalogSeriesOptionsData>() const;
template<> QStringList SeriesOptionsRegistry::visibleKeys<DigitalEventSeriesOptionsData>() const;
template<> QStringList SeriesOptionsRegistry::visibleKeys<DigitalIntervalSeriesOptionsData>() const;

// count() specializations
template<> int SeriesOptionsRegistry::count<AnalogSeriesOptionsData>() const;
template<> int SeriesOptionsRegistry::count<DigitalEventSeriesOptionsData>() const;
template<> int SeriesOptionsRegistry::count<DigitalIntervalSeriesOptionsData>() const;

// notifyChanged() specializations
template<> void SeriesOptionsRegistry::notifyChanged<AnalogSeriesOptionsData>(QString const & key);
template<> void SeriesOptionsRegistry::notifyChanged<DigitalEventSeriesOptionsData>(QString const & key);
template<> void SeriesOptionsRegistry::notifyChanged<DigitalIntervalSeriesOptionsData>(QString const & key);

#endif // SERIES_OPTIONS_REGISTRY_HPP
