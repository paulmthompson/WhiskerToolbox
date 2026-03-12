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
    QLabel * _status_label = nullptr;

    bool _restoring = false;///< Guard against recursive updates during restore
};

#endif// DATA_SYNTHESIZER_PROPERTIES_WIDGET_HPP
