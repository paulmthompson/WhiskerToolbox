#ifndef WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
#define WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"
#include "DataManagerTypes.hpp"

#include <QString>
#include <QWidget>

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


class DataTransform_Widget : public QWidget {
    Q_OBJECT
public:
    DataTransform_Widget(std::shared_ptr<DataManager> data_manager,
                         QWidget * parent = nullptr);
    ~DataTransform_Widget() override;

    void openWidget();// Call

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

    void _initializeParameterWidgetFactories();
    void _displayParameterWidget(std::string const & op_name);
    
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
};


#endif//WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
