#ifndef FEATUREPROCESSINGWIDGET_HPP
#define FEATUREPROCESSINGWIDGET_HPP

#include "DataManager/DataManagerTypes.hpp"
#include "ML_Widget/Transformations/TransformationsCommon.hpp"

#include <QString>
#include <QWidget>

#include <map>
#include <string>
#include <vector>

namespace Ui {
class FeatureProcessingWidget;
}

class DataManager;
class QTableWidgetItem;

class FeatureProcessingWidget : public QWidget {
    Q_OBJECT

public:
    using TransformationType = WhiskerTransformations::TransformationType;// Alias if preferred
    using AppliedTransformation = WhiskerTransformations::AppliedTransformation;
    using ProcessedFeatureInfo = WhiskerTransformations::ProcessedFeatureInfo;

    explicit FeatureProcessingWidget(QWidget * parent = nullptr);
    ~FeatureProcessingWidget();

    void setDataManager(DataManager * dm);
    void populateBaseFeatures();
    std::vector<WhiskerTransformations::ProcessedFeatureInfo> getActiveProcessedFeatures() const;
    bool isZScoreNormalizationEnabled() const;

signals:
    void configurationChanged();// Emitted when a feature's transformation config changes

private slots:
    void _onBaseFeatureSelectionChanged(QTableWidgetItem * current, QTableWidgetItem * previous);
    void _onIdentityCheckBoxToggled(bool checked);
    void _onSquaredCheckBoxToggled(bool checked);
    void _onLagLeadCheckBoxToggled(bool checked);
    void _onMinLagChanged(int value);
    void _onMaxLeadChanged(int value);
    void _updateActiveFeaturesDisplay();

private:
    void _updateUIForSelectedFeature();
    void _clearTransformationUI(bool clearSelectedLabel = true);
    void _addOrUpdateTransformation(WhiskerTransformations::TransformationType type, bool active, WhiskerTransformations::ParametersVariant const & params);
    void _removeTransformation(WhiskerTransformations::TransformationType type);

    Ui::FeatureProcessingWidget * ui;
    DataManager * _data_manager = nullptr;
    QString _currently_selected_base_feature_key;

    std::map<std::string, std::vector<WhiskerTransformations::AppliedTransformation>> _feature_configs;

    // Define which data types are compatible for feature processing
    std::vector<DM_DataType> const _compatible_data_types = {
            DM_DataType::Analog,
            DM_DataType::DigitalInterval,
            DM_DataType::Points,
            DM_DataType::Tensor};
};

#endif// FEATUREPROCESSINGWIDGET_HPP
