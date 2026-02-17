#ifndef TRANSFORMS_V2_STATE_HPP
#define TRANSFORMS_V2_STATE_HPP

/**
 * @file TransformsV2State.hpp
 * @brief State class for TransformsV2_Widget
 *
 * TransformsV2State manages the serializable state for the TransformsV2 properties widget,
 * enabling workspace save/restore and inter-widget communication via SelectionContext.
 *
 * @see EditorState for base class documentation
 * @see SelectionContext for inter-widget communication
 */

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <string>

class DataManager;

/**
 * @brief Serializable data structure for TransformsV2State
 *
 * This struct is designed for reflect-cpp serialization.
 * All members should be default-constructible and serializable.
 */
struct TransformsV2StateData {
    std::string instance_id;                          ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Transforms V2";       ///< User-visible name
};

/**
 * @brief State class for TransformsV2_Widget
 *
 * Manages state for the TransformsV2 properties panel which provides
 * a UI for the TransformsV2 transform pipeline architecture.
 */
class TransformsV2State : public EditorState {
    Q_OBJECT

public:
    explicit TransformsV2State(std::shared_ptr<DataManager> data_manager = nullptr,
                               QObject * parent = nullptr);
    ~TransformsV2State() override = default;

    // === Type Identification ===
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TransformsV2Widget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Accessors ===
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

signals:

private:
    std::shared_ptr<DataManager> _data_manager;
    TransformsV2StateData _data;
};

#endif // TRANSFORMS_V2_STATE_HPP
