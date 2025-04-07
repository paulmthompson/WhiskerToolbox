
#include "CSV_Loaders.hpp"
#include <fstream>
#include <sstream>

namespace Loader {

std::vector<float> loadSingleColumnCSV(CSVSingleColumnOptions const & opts) {
    std::vector<float> data;
    std::ifstream file(opts.filename);
    std::string line;

    // Skip header if specified
    if (opts.skip_header) {
        std::getline(file, line, opts.delimiter[0]);
    }

    while (std::getline(file, line, opts.delimiter[0])) {
        std::stringstream ss(line);
        float value;
        ss >> value;
        data.push_back(value);
    }

    return data;
}

std::vector<std::pair<float, float>> loadPairColumnCSV(CSVPairColumnOptions const & opts) {
    std::vector<std::pair<float, float>> data;
    std::ifstream file(opts.filename);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        std::vector<std::string> tokens;

        while (std::getline(ss, item, opts.col_delimiter[0])) {
            tokens.push_back(item);
        }

        if (tokens.size() >= 2) {
            float first = std::stof(tokens[0]);
            float second = std::stof(tokens[1]);
            if (opts.flip_column_order) {
                std::swap(first, second);
            }
            data.emplace_back(first, second);
        }
    }

    return data;
}

std::map<int, std::vector<float>> loadMultiColumnCSV(CSVMultiColumnOptions const & opts) {
    std::map<int, std::vector<float>> data;

    std::ifstream file(opts.filename);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string item;
        std::vector<std::string> tokens;

        while (std::getline(ss, item, opts.col_delimiter[0])) {
            tokens.push_back(item);
        }

        if (tokens.size() >= 2) {
            int const key = std::stoi(tokens[opts.key_column]);
            float const value = std::stof(tokens[opts.value_column]);

            data[key].push_back(value);
        }
    }

    return data;
}


}// namespace Loader
