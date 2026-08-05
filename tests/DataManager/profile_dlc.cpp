#include <iostream>
#include <string>
#include "IO/formats/CSV/points/Point_Data_CSV.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <csv_file>\n";
        return 1;
    }
    std::string filepath = argv[1];
    DLCPointLoaderOptions opts;
    opts.filepath = filepath;
    
    std::cout << "Loading " << filepath << " 5000 times for profiling..." << std::endl;
    for (int i = 0; i < 5000; ++i) {
        auto result = load_dlc_csv(opts);
        if (result.empty()) {
            std::cerr << "Failed to load or empty result on iteration " << i << "\n";
            return 1;
        }
    }
    std::cout << "Done." << std::endl;
    return 0;
}
