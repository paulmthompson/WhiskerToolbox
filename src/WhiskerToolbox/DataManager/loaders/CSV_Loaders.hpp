#ifndef BEHAVIORTOOLBOX_CSV_LOADERS_HPP
#define BEHAVIORTOOLBOX_CSV_LOADERS_HPP

#include <string>
#include <vector>

namespace Loader {

struct CSVSingleColumnOptions {
    std::string filename;
    std::string delimiter = "\n";
    bool skip_header = false;
};

struct CSVMultiColumnOptions {
    std::string filename;
    std::string line_delimiter = "\n";
    std::string col_delimiter = ",";
    bool skip_header = false;
};

std::vector<float> loadSingleColumnCSV(CSVSingleColumnOptions const & opts);

std::vector<std::pair<float, float>> loadPairColumnCSV(CSVMultiColumnOptions const & opts);

}// namespace Loader


#endif//BEHAVIORTOOLBOX_CSV_LOADERS_HPP
