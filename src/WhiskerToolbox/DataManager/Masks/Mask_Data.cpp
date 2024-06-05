#include "Mask_Data.hpp"

#include <iostream>
#include <string>

MaskData::MaskData()
{

}

void MaskData::clearMasksAtTime(int const time)
{
    _data[time].clear();
}

void MaskData::addMaskAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_mask = _createMask(x,y);

    _data[time].push_back(new_mask);
}

void MaskData::addMaskAtTime(int const time, std::vector<Point2D> const mask)
{
    _data[time].push_back(mask);
}

std::vector<Mask2D> const& MaskData::getMasksAtTime(int const time) const
{
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end())
    {
        return _data.at(time);
    } else {
        std::cout << "Error. Mask does not exist at time " << std::to_string(time) << std::endl;
    }
}

Mask2D MaskData::_createMask(std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_mask = Mask2D{x.size()};

    for (int i = 0; i < x.size(); i++)
    {
        new_mask[i] = Point2D{x[i],y[i]};
    }

    return new_mask;
}
