#ifndef HDF5_DATA_HPP
#define HDF5_DATA_HPP

#include "Media/Media_Data.hpp"

#include <string>
#include <vector>


class HDF5Data : public MediaData {
public:
    HDF5Data();
    std::string GetFrameID(int frame_id) override;

    int getFrameIndexFromNumber(int frame_id) override {return frame_id;};
protected:
    void doLoadMedia(std::string name) override;
    void doLoadFrame(int frame_id) override;
private:
    std::vector<uint16_t> _data;
    uint16_t _max_val;
};


#endif // HDF5_DATA_HPP
