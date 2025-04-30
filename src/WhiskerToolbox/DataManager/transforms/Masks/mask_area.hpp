#ifndef WHISKERTOOLBOX_MASK_AREA_HPP
#define WHISKERTOOLBOX_MASK_AREA_HPP


#include <memory>

class AnalogTimeSeries;
class MaskData;

/**
 * @brief Calculate the area of masks at each timestamp
 *
 * @param mask_data The mask data to calculate areas from
 * @return A new AnalogTimeSeries containing area values at each timestamp
 */
std::shared_ptr<AnalogTimeSeries> area(MaskData const * mask_data);


#endif//WHISKERTOOLBOX_MASK_AREA_HPP
