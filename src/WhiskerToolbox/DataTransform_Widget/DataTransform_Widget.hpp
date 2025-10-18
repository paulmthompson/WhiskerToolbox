#ifndef WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
#define WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManagerTypes.hpp"
#include "Collapsible_Widget/Section.hpp"

#include <QString>
#include <QScrollArea>
#include <QResizeEvent>
#include <QGroupBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>

#include <map>
#include <memory>
#include <string>

namespace Ui {
class DataTransform_Widget;
}

class DataManager;
class TransformOperation;
class TransformParameter_Widget;
class TransformRegistry;
class TransformPipeline;


class DataTransform_Widget : public QScrollArea {
    Q_OBJECT
public:
    DataTransform_Widget(std::shared_ptr<DataManager> data_manager,
                         QWidget * parent = nullptr);
    ~DataTransform_Widget() override;

    void openWidget();// Call

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    Ui::DataTransform_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<TransformRegistry> _registry;
    QString _highlighted_available_feature;
    std::map<std::string, std::function<TransformParameter_Widget *(QWidget *)>> _parameterWidgetFactories;
    TransformParameter_Widget * _currentParameterWidget = nullptr;
    TransformOperation * _currentSelectedOperation = nullptr;
    DataTypeVariant _currentSelectedDataVariant;
    int _current_progress = 0;

    // JSON Pipeline members
    Section* _jsonPipelineSection;
    QPushButton* _loadJsonButton;
    QTextEdit* _jsonTextEdit;
    QLabel* _jsonStatusLabel;
    QPushButton* _executeJsonButton;
    QProgressBar* _pipelineProgressBar;
    
    QString _currentJsonFile;
    std::unique_ptr<TransformPipeline> _pipeline;

    // Add these members to prevent unwanted scrolling
    int _savedScrollPosition = 0;
    bool _preventScrolling = false;

    void _initializeParameterWidgetFactories();
    void _displayParameterWidget(std::string const & op_name);
    
    // JSON Pipeline helper methods
    void _setupJsonPipelineUI();
    void _updateExecuteButtonState();
    QString _getCurrentJsonContent() const;
    void _updateJsonDisplay(const QString& jsonFilePath);
    
    /**
     * @brief Generates an output name based on the input feature and transform operation
     * @return QString containing the generated output name, or empty string if either input is missing
     */
    [[nodiscard]] QString _generateOutputName() const;
    
    /**
     * @brief Updates the output name edit box with the automatically generated name
     */
    void _updateOutputName();

private slots:
    void _handleFeatureSelected(QString const & feature);
    void _doTransform();
    void _onOperationSelected(int index);
    void _updateProgress(int progress);
    
    // JSON Pipeline slots
    void _loadJsonPipeline();
    void _onJsonTextChanged();
    void _executeJsonPipeline();
    void _validateJsonSyntax();
};


#endif//WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
