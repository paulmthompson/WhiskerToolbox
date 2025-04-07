#ifndef BEHAVIORTOOLBOX_CSV_LOADERS_HPP
#define BEHAVIORTOOLBOX_CSV_LOADERS_HPP

#include <map>
#include <string>
#include <vector>

namespace Loader {

struct CSVSingleColumnOptions {
    std::string filename;
    std::string delimiter = "\n";
    bool skip_header = false;
};

struct CSVPairColumnOptions {
    std::string filename;
    std::string line_delimiter = "\n";
    std::string col_delimiter = ",";
    bool skip_header = false;
    bool flip_column_order = false;
};

struct CSVMultiColumnOptions {
    std::string filename;
    std::string line_delimiter = "\n";
    std::string col_delimiter = ",";
    bool skip_header = false;
    size_t key_column = 0;
    size_t value_column = 1;
};

std::vector<float> loadSingleColumnCSV(CSVSingleColumnOptions const & opts);

std::vector<std::pair<float, float>> loadPairColumnCSV(CSVPairColumnOptions const & opts);

std::map<int, std::vector<float>> loadMultiColumnCSV(CSVMultiColumnOptions const & opts);

}// namespace Loader


#endif//BEHAVIORTOOLBOX_CSV_LOADERS_HPP
