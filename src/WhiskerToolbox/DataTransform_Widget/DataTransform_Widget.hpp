#ifndef WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
#define WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManagerTypes.hpp"
#include "EditorState/DataFocusAware.hpp"

#include <QString>
#include <QScrollArea>

#include <map>
#include <memory>
#include <string>

namespace Ui {
class DataTransform_Widget;
}

class DataManager;
class DataTransformWidgetState;
class QLabel;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QTextEdit;
class Section;
class SelectionContext;
struct SelectionSource;
class TransformOperation;
class TransformParameter_Widget;
class TransformRegistry;
class TransformPipeline;
class EditorRegistry;


class DataTransform_Widget : public QScrollArea, public DataFocusAware {
    Q_OBJECT
public:
    /**
     * @brief Construct DataTransform_Widget
     * @param data_manager Shared pointer to the DataManager
     * @param editor_registry Pointer to the EditorRegistry for state registration and SelectionContext access
     * @param parent Parent widget
     */
    DataTransform_Widget(std::shared_ptr<DataManager> data_manager,
                         EditorRegistry * editor_registry = nullptr,
                         QWidget * parent = nullptr);
    ~DataTransform_Widget() override;

    void openWidget();// Call

    // === DataFocusAware Interface ===
    /**
     * @brief Handle data focus changes from SelectionContext
     * 
     * This is the Phase 4.2 implementation of passive awareness.
     * When data is focused in another widget (e.g., DataManager_Widget),
     * this method updates the transform input selection.
     * 
     * @param data_key The newly focused data key
     * @param data_type The type of the focused data (e.g., "LineData", "MaskData")
     */
    void onDataFocusChanged(EditorLib::SelectedDataKey const & data_key,
                            QString const & data_type) override;

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

    // === Phase 2.7: Editor State Integration ===
    // State and workspace integration for SelectionContext-based input selection
    EditorRegistry * _editor_registry = nullptr;
    std::shared_ptr<DataTransformWidgetState> _state;
    SelectionContext * _selection_context = nullptr;

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
    void _doTransform();
    void _onOperationSelected(int index);
    void _updateProgress(int progress);
    
    // JSON Pipeline slots
    void _loadJsonPipeline();
    void _onJsonTextChanged();
    void _executeJsonPipeline();
    void _validateJsonSyntax();
    
    // === Phase 2.7: SelectionContext integration ===
    /**
     * @brief Handle external selection changes from SelectionContext
     * 
     * This is the key Phase 2.7 integration point. When data is selected
     * in another widget (e.g., DataManager_Widget), this slot updates
     * the transform input selection.
     * 
     * @param source Information about which widget made the selection
     */
    void _onExternalSelectionChanged(SelectionSource const & source);
};


#endif//WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
