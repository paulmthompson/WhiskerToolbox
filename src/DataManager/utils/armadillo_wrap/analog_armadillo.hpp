#ifndef ANALOG_ARMADILLO_HPP
#define ANALOG_ARMADILLO_HPP

#include "TimeFrame/StrongTimeTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <armadillo>


class AnalogTimeSeries;

/**
 * Convert an AnalogTimeSeries to an mlpack row vector
 * @param analogTimeSeries The AnalogTimeSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
arma::Row<double> convertAnalogTimeSeriesToMlpackArray(
        AnalogTimeSeries const * analogTimeSeries,
        std::vector<size_t> const & timestamps);

/**
 * Update an AnalogTimeSeries from an mlpack row vector
 * @param array The mlpack row vector
 * @param timestamps The timestamps to update
 * @param analogTimeSeries The AnalogTimeSeries to update
 */
void updateAnalogTimeSeriesFromMlpackArray(
        arma::Row<double> const & array,
        std::vector<TimeFrameIndex> & timestamps,
        AnalogTimeSeries * analogTimeSeries);

#endif// ANALOG_ARMADILLO_HPP
