#ifndef DATAMANAGER_H
#define DATAMANAGER_H

/*

The data manager keeps a list of the data objects as well as time objects
It keeps track of the time that is displayed and ensures that each data member that should be is rendered

*/

#include <memory>
#include <vector>
#include <string>


#include "DataSeries.h"
#include "TimeFrame.h"

class DataManager {
public:

    std::shared_ptr<VideoSeries> loadVideo(std::string);
    int getFrameCount() const {return this->frame_count;};
protected:
    std::vector<std::shared_ptr<DataSeries>> data;
    int frame_count;
};


//Load data object
// If data object has new new time series, load it
// If new time series, update scrollbar?



#endif // DATAMANAGER_H
