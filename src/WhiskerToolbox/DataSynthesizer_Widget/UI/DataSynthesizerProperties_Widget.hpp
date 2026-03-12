/**
 * @file DataSynthesizerProperties_Widget.hpp
 * @brief Properties panel for the DataSynthesizer widget
 *
 * Provides output type selection, generator selection, auto-generated
 * parameter form (via AutoParamWidget), output key configuration,
 * Preview button for ephemeral signal display, and Generate button
 * for committing data to DataManager.
 */

#ifndef DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP
#define DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <string>

class AnalogTimeSeries;
class AutoParamWidget;
class DataManager;
class DataSynthesizerState;
class QComboBox;
class QLabel;
class QLineEdit;

namespace commands {
class CommandRecorder;
}// namespace commands

/**
 * @brief Properties panel for the Data Synthesizer
 *
 * Houses output type combo, generator combo, AutoParamWidget for
 * generator-specific parameters, output key field, Preview and Generate buttons.
 */
class DataSynthesizerProperties_Widget : public QWidget {
    Q_OBJECT

public:
    explicit DataSynthesizerProperties_Widget(
            std::shared_ptr<DataSynthesizerState> state,
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);

    ~DataSynthesizerProperties_Widget() override = default;

    /**
     * @brief Set the CommandRecorder for recording command executions
     * @param recorder Non-owning pointer to the CommandRecorder (can be nullptr)
     */
    void setCommandRecorder(commands::CommandRecorder * recorder) { _command_recorder = recorder; }

signals:
    /**
     * @brief Emitted when the user requests a preview of the generated signal
     *
     * Connected to DataSynthesizerView_Widget::setPreviewData in registration.
     *
     * @param series The generated AnalogTimeSeries to preview
     */
    void previewRequested(std::shared_ptr<AnalogTimeSeries> series);

private slots:
    void _onOutputTypeChanged(int index);
    void _onGeneratorChanged(int index);
    void _onPreviewClicked();
    void _onGenerateClicked();

private:
    void _populateOutputTypes();
    void _populateGenerators(std::string const & output_type);
    void _setupGeneratorParams(std::string const & generator_name);
    void _restoreFromState();

    std::shared_ptr<DataSynthesizerState> _state;
    std::shared_ptr<DataManager> _data_manager;

    QComboBox * _output_type_combo = nullptr;
    QComboBox * _generator_combo = nullptr;
    AutoParamWidget * _auto_param_widget = nullptr;
    QLineEdit * _output_key_edit = nullptr;
    QLineEdit * _time_key_edit = nullptr;
    QComboBox * _time_frame_mode_combo = nullptr;
    QLabel * _status_label = nullptr;

    commands::CommandRecorder * _command_recorder{nullptr};

    bool _restoring = false;///< Guard against recursive updates during restore
};

#endif// DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP
