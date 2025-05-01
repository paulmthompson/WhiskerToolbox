#ifndef WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
#define WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP

#include <QString>
#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class DataTransform_Widget;
}

class DataManager;
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


private slots:
    void _handleFeatureSelected(QString const & feature);
};


#endif//WHISKERTOOLBOX_DATATRANSFORM_WIDGET_HPP
