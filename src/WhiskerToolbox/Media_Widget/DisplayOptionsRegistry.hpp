#ifndef DISPLAY_OPTIONS_REGISTRY_HPP
#define DISPLAY_OPTIONS_REGISTRY_HPP

/**
 * @file DisplayOptionsRegistry.hpp
 * @brief Generic type-safe registry for display options
 * 
 * This class provides a unified API for managing all display option types
 * (Line, Mask, Point, Tensor, Interval, Media) instead of having separate
 * methods for each type in MediaWidgetState.
 * 
 * ## Design Goals
 * 
 * 1. **Type safety**: Template methods ensure compile-time type checking
 * 2. **Single API**: One set of methods (set, get, remove, keys) for all types
 * 3. **Signal consolidation**: Three signals cover all change types
 * 4. **Non-owning**: Points to MediaWidgetStateData owned by MediaWidgetState
 * 
 * ## Usage
 * 
 * ```cpp
 * // Through MediaWidgetState
 * auto& registry = state->displayOptions();
 * 
 * // Set options (type inferred from argument)
 * LineDisplayOptions line_opts;
 * line_opts.line_thickness = 3;
 * registry.set("whisker_1", line_opts);
 * 
 * // Get options (type specified explicitly)
 * auto const* opts = registry.get<LineDisplayOptions>("whisker_1");
 * 
 * // Check existence
 * if (registry.has<LineDisplayOptions>("whisker_1")) { ... }
 * 
 * // Get all keys for a type
 * QStringList line_keys = registry.keys<LineDisplayOptions>();
 * 
 * // Get only visible/enabled keys
 * QStringList enabled = registry.enabledKeys<LineDisplayOptions>();
 * ```
 * 
 * @see MediaWidgetState
 * @see MediaWidgetStateData
 * @see DisplayOptions.hpp
 */

#include "DisplayOptions/DisplayOptions.hpp"
#include "MediaWidgetStateData.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Generic registry for all display option types
 * 
 * Replaces the separate set*Options/get*Options/remove*Options methods
 * in MediaWidgetState with a single type-safe API.
 */
class DisplayOptionsRegistry : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a DisplayOptionsRegistry
     * @param data Non-owning pointer to the underlying state data
     * @param parent Parent QObject (typically MediaWidgetState)
     */
    explicit DisplayOptionsRegistry(MediaWidgetStateData * data, QObject * parent = nullptr);

    ~DisplayOptionsRegistry() override = default;

    // === Generic Type-Safe API ===

    /**
     * @brief Set display options for a key
     * 
     * The type is inferred from the options argument.
     * Emits optionsChanged signal.
     * 
     * @tparam T Display options type (LineDisplayOptions, MaskDisplayOptions, etc.)
     * @param key The data key (e.g., "whisker_1")
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
     * @return List of enabled/visible keys
     */
    template<typename T>
    [[nodiscard]] QStringList enabledKeys() const;

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
     * @param type_name Type name ("line", "mask", "point", "tensor", "interval", "media")
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
     * @return Type name ("line", "mask", "point", "tensor", "interval", "media")
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
     * @param type_name The type name ("line", "mask", etc.)
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
    MediaWidgetStateData * _data;  // Non-owning pointer
};

// === Template Specialization Declarations ===

// Type name specializations
template<> QString DisplayOptionsRegistry::typeName<LineDisplayOptions>();
template<> QString DisplayOptionsRegistry::typeName<MaskDisplayOptions>();
template<> QString DisplayOptionsRegistry::typeName<PointDisplayOptions>();
template<> QString DisplayOptionsRegistry::typeName<TensorDisplayOptions>();
template<> QString DisplayOptionsRegistry::typeName<DigitalIntervalDisplayOptions>();
template<> QString DisplayOptionsRegistry::typeName<MediaDisplayOptions>();

// set() specializations
template<> void DisplayOptionsRegistry::set<LineDisplayOptions>(QString const & key, LineDisplayOptions const & options);
template<> void DisplayOptionsRegistry::set<MaskDisplayOptions>(QString const & key, MaskDisplayOptions const & options);
template<> void DisplayOptionsRegistry::set<PointDisplayOptions>(QString const & key, PointDisplayOptions const & options);
template<> void DisplayOptionsRegistry::set<TensorDisplayOptions>(QString const & key, TensorDisplayOptions const & options);
template<> void DisplayOptionsRegistry::set<DigitalIntervalDisplayOptions>(QString const & key, DigitalIntervalDisplayOptions const & options);
template<> void DisplayOptionsRegistry::set<MediaDisplayOptions>(QString const & key, MediaDisplayOptions const & options);

// get() specializations
template<> LineDisplayOptions const * DisplayOptionsRegistry::get<LineDisplayOptions>(QString const & key) const;
template<> MaskDisplayOptions const * DisplayOptionsRegistry::get<MaskDisplayOptions>(QString const & key) const;
template<> PointDisplayOptions const * DisplayOptionsRegistry::get<PointDisplayOptions>(QString const & key) const;
template<> TensorDisplayOptions const * DisplayOptionsRegistry::get<TensorDisplayOptions>(QString const & key) const;
template<> DigitalIntervalDisplayOptions const * DisplayOptionsRegistry::get<DigitalIntervalDisplayOptions>(QString const & key) const;
template<> MediaDisplayOptions const * DisplayOptionsRegistry::get<MediaDisplayOptions>(QString const & key) const;

// getMutable() specializations
template<> LineDisplayOptions * DisplayOptionsRegistry::getMutable<LineDisplayOptions>(QString const & key);
template<> MaskDisplayOptions * DisplayOptionsRegistry::getMutable<MaskDisplayOptions>(QString const & key);
template<> PointDisplayOptions * DisplayOptionsRegistry::getMutable<PointDisplayOptions>(QString const & key);
template<> TensorDisplayOptions * DisplayOptionsRegistry::getMutable<TensorDisplayOptions>(QString const & key);
template<> DigitalIntervalDisplayOptions * DisplayOptionsRegistry::getMutable<DigitalIntervalDisplayOptions>(QString const & key);
template<> MediaDisplayOptions * DisplayOptionsRegistry::getMutable<MediaDisplayOptions>(QString const & key);

// remove() specializations
template<> bool DisplayOptionsRegistry::remove<LineDisplayOptions>(QString const & key);
template<> bool DisplayOptionsRegistry::remove<MaskDisplayOptions>(QString const & key);
template<> bool DisplayOptionsRegistry::remove<PointDisplayOptions>(QString const & key);
template<> bool DisplayOptionsRegistry::remove<TensorDisplayOptions>(QString const & key);
template<> bool DisplayOptionsRegistry::remove<DigitalIntervalDisplayOptions>(QString const & key);
template<> bool DisplayOptionsRegistry::remove<MediaDisplayOptions>(QString const & key);

// has() specializations
template<> bool DisplayOptionsRegistry::has<LineDisplayOptions>(QString const & key) const;
template<> bool DisplayOptionsRegistry::has<MaskDisplayOptions>(QString const & key) const;
template<> bool DisplayOptionsRegistry::has<PointDisplayOptions>(QString const & key) const;
template<> bool DisplayOptionsRegistry::has<TensorDisplayOptions>(QString const & key) const;
template<> bool DisplayOptionsRegistry::has<DigitalIntervalDisplayOptions>(QString const & key) const;
template<> bool DisplayOptionsRegistry::has<MediaDisplayOptions>(QString const & key) const;

// keys() specializations
template<> QStringList DisplayOptionsRegistry::keys<LineDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::keys<MaskDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::keys<PointDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::keys<TensorDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::keys<DigitalIntervalDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::keys<MediaDisplayOptions>() const;

// enabledKeys() specializations
template<> QStringList DisplayOptionsRegistry::enabledKeys<LineDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::enabledKeys<MaskDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::enabledKeys<PointDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::enabledKeys<TensorDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::enabledKeys<DigitalIntervalDisplayOptions>() const;
template<> QStringList DisplayOptionsRegistry::enabledKeys<MediaDisplayOptions>() const;

// count() specializations
template<> int DisplayOptionsRegistry::count<LineDisplayOptions>() const;
template<> int DisplayOptionsRegistry::count<MaskDisplayOptions>() const;
template<> int DisplayOptionsRegistry::count<PointDisplayOptions>() const;
template<> int DisplayOptionsRegistry::count<TensorDisplayOptions>() const;
template<> int DisplayOptionsRegistry::count<DigitalIntervalDisplayOptions>() const;
template<> int DisplayOptionsRegistry::count<MediaDisplayOptions>() const;

// notifyChanged() specializations
template<> void DisplayOptionsRegistry::notifyChanged<LineDisplayOptions>(QString const & key);
template<> void DisplayOptionsRegistry::notifyChanged<MaskDisplayOptions>(QString const & key);
template<> void DisplayOptionsRegistry::notifyChanged<PointDisplayOptions>(QString const & key);
template<> void DisplayOptionsRegistry::notifyChanged<TensorDisplayOptions>(QString const & key);
template<> void DisplayOptionsRegistry::notifyChanged<DigitalIntervalDisplayOptions>(QString const & key);
template<> void DisplayOptionsRegistry::notifyChanged<MediaDisplayOptions>(QString const & key);

#endif // DISPLAY_OPTIONS_REGISTRY_HPP
