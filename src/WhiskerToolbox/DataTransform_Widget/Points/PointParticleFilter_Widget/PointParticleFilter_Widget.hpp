#ifndef POINTPARTICLEFILTER_WIDGET_HPP
#define POINTPARTICLEFILTER_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/DataManagerParameter_Widget.hpp"

#include <memory>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;

namespace Ui {
class PointParticleFilter_Widget;
}

/**
 * @brief Widget for configuring Point Particle Filter parameters
 * 
 * This widget allows users to:
 * - Select mask data to constrain tracking
 * - Configure particle filter parameters (number of particles, transition radius, random walk probability)
 * - View and select available grouped point data
 * 
 * The particle filter tracks sparse point labels through mask sequences using
 * forward filtering and backward smoothing.
 */
class PointParticleFilter_Widget : public DataManagerParameter_Widget {
    Q_OBJECT

public:
    explicit PointParticleFilter_Widget(QWidget *parent = nullptr);
    ~PointParticleFilter_Widget() override;

    /**
     * @brief Get the current parameters from the UI
     * @return Unique pointer to PointParticleFilterParameters
     */
    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;

    /**
     * @brief Set the widget's DataManager for accessing mask data
     * @param dm Shared pointer to DataManager
     */
    void setDataManager(std::shared_ptr<DataManager> dm) override;

protected:
    /**
     * @brief Update UI when DataManager changes
     */
    void onDataManagerChanged() override;

    /**
     * @brief Update UI when DataManager data changes
     */
    void onDataManagerDataChanged() override;

private slots:
    /**
     * @brief Handle mask data selection change
     */
    void onMaskDataChanged(int index);

private:
    /**
     * @brief Populate the mask data combo box with available mask datasets
     */
    void populateMaskDataComboBox();

    /**
     * @brief Update the info label with current settings
     */
    void updateInfoLabel();

    Ui::PointParticleFilter_Widget *ui;
    
    // Cached mask data key
    QString _selectedMaskDataKey;
};

#endif // POINTPARTICLEFILTER_WIDGET_HPP
