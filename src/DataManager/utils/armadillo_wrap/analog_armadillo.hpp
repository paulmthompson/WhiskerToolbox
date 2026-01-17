#ifndef ANALOG_ARMADILLO_HPP
#define ANALOG_ARMADILLO_HPP

#include "datamanager_export.h"

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
arma::Row<double> DATAMANAGER_EXPORT convertAnalogTimeSeriesToMlpackArray(
        AnalogTimeSeries const * analogTimeSeries,
        std::vector<size_t> const & timestamps);

#endif// ANALOG_ARMADILLO_HPP
