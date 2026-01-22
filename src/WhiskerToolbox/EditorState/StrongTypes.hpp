#ifndef EDITOR_STATE_STRONG_TYPES_HPP
#define EDITOR_STATE_STRONG_TYPES_HPP

/**
 * @file StrongTypes.hpp
 * @brief Strong type wrappers for EditorState identifiers
 *
 * This file provides strongly-typed wrappers around QString to prevent
 * accidental mixing of different identifier types. Using these types
 * instead of raw QString provides:
 *
 * - Compile-time safety: Can't pass a DataKey where EditorInstanceId is expected
 * - Self-documenting code: Function signatures clearly state what they need
 * - Better IDE support: Autocomplete and error messages are more helpful
 * - Refactoring safety: Type changes are caught by the compiler
 *
 * @see EditorState for widget state management
 * @see OperationContext for inter-widget operations
 */

#include <QString>
#include <QHash>
#include <QUuid>
#include <QMetaType>

#include <compare>
#include <functional>

namespace EditorLib {

/**
 * @brief CRTP base for strong string types
 *
 * Provides common operations while preventing accidental mixing
 * of different ID types. All strong types are copyable, movable,
 * and can be used as keys in both std:: and Qt containers.
 */
template<typename Derived>
struct StrongStringId {
    QString value;

    StrongStringId() = default;
    explicit StrongStringId(QString v) : value(std::move(v)) {}
    explicit StrongStringId(char const * v) : value(QString::fromUtf8(v)) {}
    explicit StrongStringId(std::string const & v) : value(QString::fromStdString(v)) {}

    [[nodiscard]] bool isEmpty() const { return value.isEmpty(); }
    [[nodiscard]] bool isValid() const { return !value.isEmpty(); }
    [[nodiscard]] QString const & toString() const { return value; }
    [[nodiscard]] std::string toStdString() const { return value.toStdString(); }

    void clear() { value.clear(); }

    // Comparison operators for std::map/set
    auto operator<=>(StrongStringId const & other) const {
        return value.compare(other.value) <=> 0;
    }
    bool operator==(StrongStringId const & other) const {
        return value == other.value;
    }

    // For Qt containers (QHash, QSet)
    friend size_t qHash(StrongStringId const & id, size_t seed = 0) {
        return qHash(id.value, seed);
    }
};

/**
 * @brief Unique identifier for an editor instance (UUID format)
 *
 * Each EditorState instance gets a unique EditorInstanceId that persists
 * across serialization/deserialization. Used for:
 * - State lookup in EditorRegistry
 * - Selection tracking
 * - Operation routing
 */
struct EditorInstanceId : StrongStringId<EditorInstanceId> {
    using StrongStringId::StrongStringId;

    /// Generate a new unique instance ID
    static EditorInstanceId generate() {
        return EditorInstanceId(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
};

/**
 * @brief Type identifier for an editor class
 *
 * Identifies the type of editor (e.g., "MediaWidget", "DataTransformWidget").
 * Used for:
 * - Factory registration
 * - Serialization type field
 * - Operation routing to producer types
 */
struct EditorTypeId : StrongStringId<EditorTypeId> {
    using StrongStringId::StrongStringId;
};

/**
 * @brief Key for selected data in SelectionContext
 *
 * Identifies selected data (e.g., "whisker_1", "emg_channel_0").
 * Used for tracking which data objects are currently selected
 * in the UI. This is separate from DataKey used in DataManager
 * to allow for QString-based operations with Qt signals/slots.
 */
struct SelectedDataKey : StrongStringId<SelectedDataKey> {
    using StrongStringId::StrongStringId;
};

/**
 * @brief Unique identifier for a pending operation
 *
 * Each operation request gets a unique OperationId for tracking
 * and cancellation.
 */
struct OperationId : StrongStringId<OperationId> {
    using StrongStringId::StrongStringId;

    /// Generate a new unique operation ID
    static OperationId generate() {
        return OperationId(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
};

/**
 * @brief Channel name for operation data flow
 *
 * Identifies what kind of data is being passed in an operation result.
 * Acts as a runtime type hint for the std::any payload.
 */
struct DataChannel : StrongStringId<DataChannel> {
    using StrongStringId::StrongStringId;
};

/// Well-known data channels
namespace DataChannels {
inline DataChannel const TransformPipeline{"transform.pipeline"};
inline DataChannel const Selection{"selection"};
inline DataChannel const DataReference{"data.reference"};
}  // namespace DataChannels

/// Well-known editor types
namespace EditorTypes {
inline EditorTypeId const DataTransformWidget{"DataTransformWidget"};
inline EditorTypeId const MediaWidget{"MediaWidget"};
inline EditorTypeId const DataManagerWidget{"DataManagerWidget"};
inline EditorTypeId const DataViewerWidget{"DataViewerWidget"};
}  // namespace EditorTypes

}  // namespace EditorLib

// std::hash specializations for use with std::unordered_map/set
namespace std {

template<>
struct hash<EditorLib::EditorInstanceId> {
    size_t operator()(EditorLib::EditorInstanceId const & id) const {
        return qHash(id.value);
    }
};

template<>
struct hash<EditorLib::EditorTypeId> {
    size_t operator()(EditorLib::EditorTypeId const & id) const {
        return qHash(id.value);
    }
};

template<>
struct hash<EditorLib::SelectedDataKey> {
    size_t operator()(EditorLib::SelectedDataKey const & id) const {
        return qHash(id.value);
    }
};

template<>
struct hash<EditorLib::OperationId> {
    size_t operator()(EditorLib::OperationId const & id) const {
        return qHash(id.value);
    }
};

template<>
struct hash<EditorLib::DataChannel> {
    size_t operator()(EditorLib::DataChannel const & id) const {
        return qHash(id.value);
    }
};

}  // namespace std

// Qt metatype registration for signal/slot use
// These must be at global scope, after the types are fully defined
Q_DECLARE_METATYPE(EditorLib::EditorInstanceId)
Q_DECLARE_METATYPE(EditorLib::EditorTypeId)
Q_DECLARE_METATYPE(EditorLib::SelectedDataKey)
Q_DECLARE_METATYPE(EditorLib::OperationId)
Q_DECLARE_METATYPE(EditorLib::DataChannel)

#endif  // EDITOR_STATE_STRONG_TYPES_HPP
