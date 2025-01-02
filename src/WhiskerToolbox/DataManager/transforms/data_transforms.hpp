
#ifndef WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
#define WHISKERTOOLBOX_DATA_TRANSFORMS_HPP

#include <memory>
#include <vector>

class MaskData;
class AnalogTimeSeries;


std::shared_ptr<AnalogTimeSeries> area(const std::shared_ptr<MaskData>& mask_data);


#endif //WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
