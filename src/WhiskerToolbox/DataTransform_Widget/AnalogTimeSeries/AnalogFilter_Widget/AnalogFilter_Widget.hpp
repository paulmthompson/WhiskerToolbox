#ifndef ANALOG_FILTER_WIDGET_HPP
#define ANALOG_FILTER_WIDGET_HPP

#include "DataTransform_Widget/TransformParameter_Widget/TransformParameter_Widget.hpp"

#include <memory>

// Forward declarations
class IFilter;
class AnalogFilterParams;

namespace Ui { class AnalogFilter_Widget; }

class AnalogFilter_Widget : public TransformParameter_Widget {
    Q_OBJECT

public:
    explicit AnalogFilter_Widget(QWidget * parent = nullptr);
    ~AnalogFilter_Widget() override;

    [[nodiscard]] std::unique_ptr<TransformParametersBase> getParameters() const override;
    
    /**
     * @brief Get parameters using the new filter interface (for better performance)
     * @return Modern filter parameters with pre-created filter instance
     */
    [[nodiscard]] std::unique_ptr<AnalogFilterParams> getModernParameters() const;

private slots:
    void _onFilterTypeChanged(int index);
    void _onResponseChanged(int index);
    void _validateParameters();
    void _updateVisibleParameters();

private:
    std::unique_ptr<Ui::AnalogFilter_Widget> ui;
    void _setupConnections();
    
    // Helper methods for creating filters with runtime order selection
    std::unique_ptr<IFilter> createButterworthLowpassByOrder(int order, double cutoff_hz, double sampling_rate_hz, bool zero_phase) const;
    std::unique_ptr<IFilter> createButterworthHighpassByOrder(int order, double cutoff_hz, double sampling_rate_hz, bool zero_phase) const;
    std::unique_ptr<IFilter> createButterworthBandpassByOrder(int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, bool zero_phase) const;
    std::unique_ptr<IFilter> createButterworthBandstopByOrder(int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, bool zero_phase) const;
    
    std::unique_ptr<IFilter> createChebyshevILowpassByOrder(int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    std::unique_ptr<IFilter> createChebyshevIHighpassByOrder(int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    std::unique_ptr<IFilter> createChebyshevIBandpassByOrder(int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    std::unique_ptr<IFilter> createChebyshevIBandstopByOrder(int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    
    std::unique_ptr<IFilter> createChebyshevIILowpassByOrder(int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    std::unique_ptr<IFilter> createChebyshevIIHighpassByOrder(int order, double cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    std::unique_ptr<IFilter> createChebyshevIIBandpassByOrder(int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
    std::unique_ptr<IFilter> createChebyshevIIBandstopByOrder(int order, double low_cutoff_hz, double high_cutoff_hz, double sampling_rate_hz, double ripple_db, bool zero_phase) const;
}; 

#endif // ANALOG_FILTER_WIDGET_HPP