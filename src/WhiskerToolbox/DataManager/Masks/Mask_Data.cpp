#include "Mask_Data.hpp"

#include <iostream>
#include <string>

MaskData::MaskData() :
    _mask_height{256},
    _mask_width{256}
{

}

void MaskData::clearMasksAtTime(int const time)
{
    _data[time].clear();
    //NOTIFY
}

void MaskData::addMaskAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y)
{
    auto new_mask = create_mask(x,y);

    _data[time].push_back(new_mask);
    //NOTIFY
}

void MaskData::addMaskAtTime(int const time, std::vector<Point2D<float>> const & mask)
{
    _data[time].push_back(std::move(mask));
    //NOTIFY
}

std::vector<Mask2D> const& MaskData::getMasksAtTime(int const time) const
{
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end())
    {
        return _data.at(time);
    } else {
        return _empty;
    }
}
