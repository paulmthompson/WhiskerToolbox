#ifndef EDITOR_STATE_HPP
#define EDITOR_STATE_HPP

/**
 * @file EditorState.hpp
 * @brief Base class for serializable widget/editor state
 * 
 * EditorState provides a common interface for managing the state of
 * widgets/editors in WhiskerToolbox. It supports:
 * - JSON serialization via reflect-cpp
 * - Unique instance identification
 * - Dirty state tracking
 * - Qt signal-based change notification
 * 
 * @see WorkspaceManager for state registry
 * @see SelectionContext for inter-widget communication
 */

#include <QObject>
#include <QString>
#include <QUuid>

#include <string>

/**
 * @brief Base class for all editor/widget states
 * 
 * EditorState is designed to separate widget visual components from their
 * underlying state. This enables:
 * 
 * 1. **Multiple views of same state**: Properties panels and main editors
 *    can both observe the same state object
 * 
 * 2. **Serialization**: Complete application state can be saved/restored
 * 
 * 3. **Undo/Redo**: Command pattern can operate on state objects
 * 
 * 4. **Testing**: State logic can be tested without UI
 * 
 * ## Usage Pattern
 * 
 * Subclasses should:
 * 
 * 1. Define state data as a reflect-cpp compatible struct:
 *    ```cpp
 *    struct MyWidgetStateData {
 *        std::string selected_item;
 *        double zoom_level = 1.0;
 *    };
 *    ```
 * 
 * 2. Inherit from EditorState and implement required methods:
 *    ```cpp
 *    class MyWidgetState : public EditorState {
 *        Q_OBJECT
 *    public:
 *        QString getTypeName() const override { return "MyWidget"; }
 *        QString getDisplayName() const override { return _display_name; }
 *        std::string toJson() const override;
 *        bool fromJson(std::string const& json) override;
 *        
 *        // Typed accessors
 *        void setSelectedItem(std::string const& item);
 *        std::string getSelectedItem() const;
 *        
 *    signals:
 *        void selectedItemChanged(QString const& item);
 *        
 *    private:
 *        MyWidgetStateData _data;
 *    };
 *    ```
 * 
 * 3. Call markDirty() in setters to track unsaved changes
 * 
 * 4. Emit specific signals for each property change
 */
class EditorState : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new EditorState
     * @param parent Parent QObject (usually nullptr, managed by WorkspaceManager)
     */
    explicit EditorState(QObject * parent = nullptr);

    /**
     * @brief Virtual destructor
     */
    ~EditorState() override = default;

    // === Type Identification ===

    /**
     * @brief Get unique type name for this editor state
     * 
     * This is used for:
     * - Factory registration
     * - Serialization type field
     * - Properties panel routing
     * 
     * @return Type name, e.g., "MediaWidget", "DataViewer"
     */
    [[nodiscard]] virtual QString getTypeName() const = 0;

    /**
     * @brief Get display name for UI
     * 
     * This is the user-visible name shown in tabs, window titles, etc.
     * It may be customized by the user.
     * 
     * @return Display name, e.g., "Media Viewer 1"
     */
    [[nodiscard]] virtual QString getDisplayName() const;

    /**
     * @brief Set display name
     * @param name New display name
     */
    virtual void setDisplayName(QString const & name);

    // === Instance Identification ===

    /**
     * @brief Get unique instance ID
     * 
     * Each EditorState instance has a UUID that persists across
     * serialization/deserialization. This enables:
     * - State lookup in WorkspaceManager
     * - Selection tracking
     * - Reference preservation during save/load
     * 
     * @return Instance ID as QString (UUID format)
     */
    [[nodiscard]] QString getInstanceId() const { return _instance_id; }

    // === Serialization ===

    /**
     * @brief Serialize state to JSON string
     * 
     * Subclasses should use reflect-cpp for serialization:
     * ```cpp
     * std::string toJson() const override {
     *     return rfl::json::write(_data);
     * }
     * ```
     * 
     * @return JSON string representation of state
     */
    [[nodiscard]] virtual std::string toJson() const = 0;

    /**
     * @brief Restore state from JSON string
     * 
     * Subclasses should use reflect-cpp for deserialization:
     * ```cpp
     * bool fromJson(std::string const& json) override {
     *     auto result = rfl::json::read<MyWidgetStateData>(json);
     *     if (result) {
     *         _data = *result;
     *         emit stateChanged();
     *         return true;
     *     }
     *     return false;
     * }
     * ```
     * 
     * @param json JSON string to parse
     * @return true if parsing succeeded, false on error
     */
    virtual bool fromJson(std::string const & json) = 0;

    // === Dirty State Tracking ===

    /**
     * @brief Check if state has unsaved changes
     * @return true if state has been modified since last markClean()
     */
    [[nodiscard]] bool isDirty() const { return _is_dirty; }

    /**
     * @brief Mark state as clean (after save)
     * 
     * Called after state has been persisted to reset dirty tracking.
     */
    void markClean();

signals:
    /**
     * @brief Emitted when any state property changes
     * 
     * This is a catch-all signal. Subclasses should also emit
     * more specific signals for individual properties.
     */
    void stateChanged();

    /**
     * @brief Emitted when display name changes
     * @param newName The new display name
     */
    void displayNameChanged(QString const & newName);

    /**
     * @brief Emitted when dirty state changes
     * @param isDirty New dirty state
     */
    void dirtyChanged(bool isDirty);

protected:
    /**
     * @brief Mark state as modified
     * 
     * Call this from setters to track unsaved changes.
     * Also emits stateChanged().
     */
    void markDirty();

    /**
     * @brief Set instance ID (for deserialization)
     * 
     * Normally the instance ID is generated in constructor,
     * but during deserialization we need to restore the original ID.
     * 
     * @param id Instance ID to set
     */
    void setInstanceId(QString const & id) { _instance_id = id; }

    /**
     * @brief Generate a new unique instance ID
     * @return New UUID as QString
     */
    static QString generateInstanceId();

private:
    QString _instance_id;
    QString _display_name;
    bool _is_dirty = false;
};

#endif // EDITOR_STATE_HPP
