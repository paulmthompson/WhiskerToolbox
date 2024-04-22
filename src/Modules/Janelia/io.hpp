#ifndef IO_HPP
#define IO_HPP

#include "janelia.h"

#include <string>
#include <iostream>
#include <fstream>

std::vector<Whisker_Seg> load_binary_data(const std::string filename)
{
    std::ifstream whisk_stream(filename, std::ifstream::binary);

    char header[] = "bwhiskbin1\0";

    whisk_stream.read(header, sizeof(header));

    std::cout << header << std::endl;

    std::vector<Whisker_Seg> wsegs = {};

    typedef struct {int id; int time; int len;} trunc_WSeg;

    trunc_WSeg *whisker_segment_header;

    while (!whisk_stream.eof()) {


        whisk_stream.read(reinterpret_cast<char*>(whisker_segment_header), sizeof(trunc_WSeg));

        if (whisk_stream.eof()) {
            break;
        }

        auto this_seg = Whisker_Seg(whisker_segment_header->len);

        this_seg.id = whisker_segment_header->id;
        this_seg.time = whisker_segment_header->time;

        whisk_stream.read(reinterpret_cast<char*>(this_seg.x.data()), sizeof(float)*this_seg.len);
        whisk_stream.read(reinterpret_cast<char*>(this_seg.y.data()), sizeof(float)*this_seg.len);
        whisk_stream.read(reinterpret_cast<char*>(this_seg.thick.data()), sizeof(float)*this_seg.len);
        whisk_stream.read(reinterpret_cast<char*>(this_seg.scores.data()), sizeof(float)*this_seg.len);

        wsegs.push_back(std::move(this_seg));

    }

    std::cout << "Loaded " << wsegs.size() << " total whiskers from file" << std::endl;
    return wsegs;
};


#endif // IO_HPP
