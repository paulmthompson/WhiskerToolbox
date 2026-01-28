#include "AnalogTimeSeriesDataView.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"

#include <QLabel>
#include <QVBoxLayout>

AnalogTimeSeriesDataView::AnalogTimeSeriesDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent) {
    _setupUi();
}

AnalogTimeSeriesDataView::~AnalogTimeSeriesDataView() {
    removeCallbacks();
}

void AnalogTimeSeriesDataView::setActiveKey(std::string const & key) {
    _active_key = key;
    updateView();
}

void AnalogTimeSeriesDataView::removeCallbacks() {
    // AnalogTimeSeries view doesn't register callbacks currently
}

void AnalogTimeSeriesDataView::updateView() {
    if (_active_key.empty() || !_info_label) {
        if (_info_label) {
            _info_label->setText(tr("No data selected"));
        }
        return;
    }

    auto analog_data = dataManager()->getData<AnalogTimeSeries>(_active_key);
    if (!analog_data) {
        _info_label->setText(tr("Data not found: %1").arg(QString::fromStdString(_active_key)));
        return;
    }

    // Build info string
    QString info;
    info += tr("<b>Analog Time Series</b><br><br>");
    info += tr("<b>Key:</b> %1<br>").arg(QString::fromStdString(_active_key));
    
    auto const data = analog_data->getAnalogTimeSeries();
    info += tr("<b>Sample Count:</b> %1<br>").arg(analog_data->getNumSamples());
    
    if (!data.empty()) {
        // Calculate basic statistics
        float min_val = data[0];
        float max_val = data[0];
        double sum = 0.0;
        
        for (float val : data) {
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
            sum += val;
        }
        
        float mean = static_cast<float>(sum / static_cast<double>(data.size()));
        
        info += tr("<br><b>Statistics:</b><br>");
        info += tr("  Min: %1<br>").arg(min_val, 0, 'f', 4);
        info += tr("  Max: %1<br>").arg(max_val, 0, 'f', 4);
        info += tr("  Mean: %1<br>").arg(mean, 0, 'f', 4);
        info += tr("  Range: %1<br>").arg(max_val - min_val, 0, 'f', 4);
    }

    info += tr("<br><i>Note: Analog time series data is continuous.<br>");
    info += tr("Use the Data Viewer widget for waveform visualization.</i>");

    _info_label->setText(info);
}

void AnalogTimeSeriesDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(20, 20, 20, 20);
    _layout->setSpacing(10);

    _info_label = new QLabel(this);
    _info_label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    _info_label->setWordWrap(true);
    _info_label->setTextFormat(Qt::RichText);
    _info_label->setText(tr("No data selected"));

    _layout->addWidget(_info_label);
    _layout->addStretch();
}
