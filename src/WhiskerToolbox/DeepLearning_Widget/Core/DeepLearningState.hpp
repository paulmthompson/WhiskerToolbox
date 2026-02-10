#ifndef DEEP_LEARNING_STATE_HPP
#define DEEP_LEARNING_STATE_HPP

/**
 * @file DeepLearningState.hpp
 * @brief Minimal state class for DeepLearning widget.
 *
 * DeepLearningState is a placeholder EditorState implementation used to
 * demonstrate the View/Properties split pattern for deep learning tools.
 * It currently does not track any custom data beyond the inherited
 * EditorState fields, but provides the required interface for
 * registration with EditorRegistry.
 *
 * @see EditorState for base class documentation.
 */

#include "EditorState/EditorState.hpp"

#include <memory>
#include <string>

/**
 * @brief Minimal state class for the DeepLearning widget.
 *
 * DeepLearningState currently has no custom serializable fields. It
 * exists to satisfy the EditorState interface so view and properties
 * widgets can share a common state object and be registered with the
 * editor registry.
 */
class DeepLearningState : public EditorState {
    Q_OBJECT

public:
    /**
     * @brief Construct a new DeepLearningState.
     *
     * @param parent Parent QObject (typically nullptr, managed by WorkspaceManager).
     */
    explicit DeepLearningState(QObject * parent = nullptr);

    /**
     * @brief Virtual destructor.
     */
    ~DeepLearningState() override = default;

    /**
     * @brief Get the type name for this state.
     *
     * @return QString Type identifier used by EditorRegistry (\"DeepLearningWidget\").
     */
    [[nodiscard]] QString getTypeName() const override;

    /**
     * @brief Serialize state to JSON.
     *
     * This minimal implementation returns an empty JSON object since the
     * state currently has no custom fields.
     *
     * @return std::string JSON string representation.
     */
    [[nodiscard]] std::string toJson() const override;

    /**
     * @brief Restore state from JSON.
     *
     * This minimal implementation ignores the input JSON and always
     * reports success, as there are no custom fields to populate.
     *
     * @param json JSON string to parse.
     * @return true Always returns true for the placeholder implementation.
     */
    bool fromJson(std::string const & json) override;
};

#endif// DEEP_LEARNING_STATE_HPP
