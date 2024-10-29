
#include "CSV_Loaders.hpp"
#include <fstream>
#include <sstream>

namespace CSVLoader {

std::vector<float> loadSingleColumnCSV(const std::string& filename) {
    std::vector<float> data;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        float value;
        ss >> value;
        data.push_back(value);
    }

    return data;
}

std::vector<std::pair<float, float>> loadPairColumnCSV(const std::string& filename) {
    std::vector<std::pair<float, float>> data;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        float first, second;
        ss >> first >> second;
        data.emplace_back(first, second);
    }

    return data;
}

} // namespace CSVLoader