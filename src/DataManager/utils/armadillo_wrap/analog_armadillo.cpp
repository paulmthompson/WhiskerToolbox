
#include "analog_armadillo.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"


arma::Row<double> convertAnalogTimeSeriesToMlpackArray(
        AnalogTimeSeries const * analogTimeSeries,
        std::vector<size_t> const & timestamps) {
    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    auto const & data = analogTimeSeries->getAnalogTimeSeries();
    auto const & time = analogTimeSeries->getTimeSeries();

    for (std::size_t i = 0; i < length; ++i) {
        auto it = std::find(time.begin(), time.end(), TimeFrameIndex(static_cast<int64_t>(timestamps[i])));
        if (it != time.end()) {
            result[i] = data[static_cast<size_t>(std::distance(time.begin(), it))];
        } else {
            result[i] = arma::datum::nan;
        }
    }

    return result;
}
