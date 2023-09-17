
#include "DataSeries.h"


VideoSeries::VideoSeries() {
    vd = std::make_unique<ffmpeg_wrapper::VideoDecoder>();
}

