#ifndef FEATUREPROCESSINGWIDGET_HPP
#define FEATUREPROCESSINGWIDGET_HPP

#include <QWidget>
#include <QString>
#include <vector>
#include <string>
#include <map>

#include "DataManager/DataManager.hpp" // For DataManager and DM_DataType

// Forward declaration of UI class
namespace Ui {
    class FeatureProcessingWidget;
}

// Forward declaration for QTableWidgetItem
class QTableWidgetItem;

class FeatureProcessingWidget : public QWidget {
    Q_OBJECT

public:
    enum class TransformationType {
        Identity // Future: Absolute, Lag, etc.
    };

    struct AppliedTransformation {
        TransformationType type;
        // std::map<std::string, QVariant> parameters; // For future transformations with params

        // For simplicity in comparison and storage, especially if no params yet
        bool operator==(const AppliedTransformation& other) const {
            return type == other.type; // && parameters == other.parameters;
        }
    };

    struct ProcessedFeatureInfo {
        std::string base_feature_key;
        AppliedTransformation transformation;
        std::string output_feature_name; 
    };

    explicit FeatureProcessingWidget(QWidget *parent = nullptr);
    ~FeatureProcessingWidget();

    void setDataManager(DataManager* dm);
    void populateBaseFeatures();
    std::vector<ProcessedFeatureInfo> getActiveProcessedFeatures() const;

signals:
    void configurationChanged(); // Emitted when a feature's transformation config changes

private slots:
    void _onBaseFeatureSelectionChanged(QTableWidgetItem* current, QTableWidgetItem* previous);
    void _onIdentityCheckBoxToggled(bool checked);
    void _updateActiveFeaturesDisplay();
    // Add slots for future transformation controls here

private:
    void _updateUIForSelectedFeature();
    void _clearTransformationUI(bool clearSelectedLabel = true);
    void _addOrUpdateTransformation(TransformationType type, bool active);
    void _removeTransformation(TransformationType type);

    Ui::FeatureProcessingWidget *ui;
    DataManager* _data_manager = nullptr;
    QString _currently_selected_base_feature_key;
    
    // Stores the configuration: base feature key -> list of transformations applied to it
    std::map<std::string, std::vector<AppliedTransformation>> _feature_configs;
    
    // Define which data types are compatible for feature processing
    const std::vector<DM_DataType> _compatible_data_types = {
        DM_DataType::Analog,
        DM_DataType::DigitalInterval,
        DM_DataType::Points, 
        DM_DataType::Tensor
    };
};

#endif // FEATUREPROCESSINGWIDGET_HPP 