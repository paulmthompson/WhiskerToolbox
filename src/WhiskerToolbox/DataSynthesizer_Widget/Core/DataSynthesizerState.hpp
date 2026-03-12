/**
 * @file DataSynthesizerState.hpp
 * @brief EditorState subclass for the DataSynthesizer widget
 *
 * Minimal state for Milestone 2a — tracks only instance_id and display_name.
 * Will be expanded in Milestone 2b with generator selection fields.
 *
 * @see DataSynthesizerStateData for the serializable POD struct
 */

#ifndef DATA_SYNTHESIZER_STATE_HPP
#define DATA_SYNTHESIZER_STATE_HPP

#include "DataSynthesizer_Widget/Core/DataSynthesizerStateData.hpp"
#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <string>

/**
 * @brief State class for DataSynthesizer widget
 *
 * Manages the serializable state of the DataSynthesizer widget.
 * Currently minimal (Milestone 2a) — stores only instance identity.
 */
class DataSynthesizerState : public EditorState {
    Q_OBJECT

public:
    explicit DataSynthesizerState(QObject * parent = nullptr);
    ~DataSynthesizerState() override = default;

    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("DataSynthesizerWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

private:
    DataSynthesizerStateData _data;
};

#endif// DATA_SYNTHESIZER_STATE_HPP
