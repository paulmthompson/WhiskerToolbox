#ifndef FEATUREPROCESSINGWIDGET_HPP
#define FEATUREPROCESSINGWIDGET_HPP

#include <QWidget>
#include <QString>
#include <vector>
#include <string>
#include <map>

#include "DataManager/DataManager.hpp" // For DataManager and DM_DataType
#include "ML_Widget/Transformations/TransformationsCommon.hpp" // Added

// Forward declaration of UI class
namespace Ui {
    class FeatureProcessingWidget;
}

// Forward declaration for QTableWidgetItem
class QTableWidgetItem;

class FeatureProcessingWidget : public QWidget {
    Q_OBJECT

public:

    using TransformationType = WhiskerTransformations::TransformationType; // Alias if preferred
    using AppliedTransformation = WhiskerTransformations::AppliedTransformation;
    using ProcessedFeatureInfo = WhiskerTransformations::ProcessedFeatureInfo;

    explicit FeatureProcessingWidget(QWidget *parent = nullptr);
    ~FeatureProcessingWidget();

    void setDataManager(DataManager* dm);
    void populateBaseFeatures();
    std::vector<WhiskerTransformations::ProcessedFeatureInfo> getActiveProcessedFeatures() const;

signals:
    void configurationChanged(); // Emitted when a feature's transformation config changes

private slots:
    void _onBaseFeatureSelectionChanged(QTableWidgetItem* current, QTableWidgetItem* previous);
    void _onIdentityCheckBoxToggled(bool checked);
    void _onSquaredCheckBoxToggled(bool checked);
    void _updateActiveFeaturesDisplay();
    // Add slots for future transformation controls here

private:
    void _updateUIForSelectedFeature();
    void _clearTransformationUI(bool clearSelectedLabel = true);
    void _addOrUpdateTransformation(WhiskerTransformations::TransformationType type, bool active, const WhiskerTransformations::ParametersVariant& params);
    void _removeTransformation(WhiskerTransformations::TransformationType type);

    Ui::FeatureProcessingWidget *ui;
    DataManager* _data_manager = nullptr;
    QString _currently_selected_base_feature_key;
    
    // Stores the configuration: base feature key -> list of transformations applied to it
    std::map<std::string, std::vector<WhiskerTransformations::AppliedTransformation>> _feature_configs;
    
    // Define which data types are compatible for feature processing
    const std::vector<DM_DataType> _compatible_data_types = {
        DM_DataType::Analog,
        DM_DataType::DigitalInterval,
        DM_DataType::Points, 
        DM_DataType::Tensor
    };
};

#endif // FEATUREPROCESSINGWIDGET_HPP 
