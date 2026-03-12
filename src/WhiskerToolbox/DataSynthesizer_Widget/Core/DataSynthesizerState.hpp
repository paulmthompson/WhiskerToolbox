/**
 * @file DataSynthesizerState.hpp
 * @brief EditorState subclass for the DataSynthesizer widget
 *
 * Manages generator selection, parameter editing, and output configuration.
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
 * Manages the serializable state of the DataSynthesizer widget,
 * including generator selection, parameters, and output configuration.
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

    // --- Generator selection ---
    [[nodiscard]] std::string const & outputType() const;
    void setOutputType(std::string const & type);

    [[nodiscard]] std::string const & generatorName() const;
    void setGeneratorName(std::string const & name);

    [[nodiscard]] std::string const & parameterJson() const;
    void setParameterJson(std::string const & json);

    // --- Output configuration ---
    [[nodiscard]] std::string const & outputKey() const;
    void setOutputKey(std::string const & key);

    [[nodiscard]] std::string const & timeKey() const;
    void setTimeKey(std::string const & key);

signals:
    void outputTypeChanged(std::string const & type);
    void generatorNameChanged(std::string const & name);
    void parameterJsonChanged(std::string const & json);
    void outputKeyChanged(std::string const & key);

private:
    DataSynthesizerStateData _data;
};

#endif// DATA_SYNTHESIZER_STATE_HPP
