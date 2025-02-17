
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
        std::string item;
        std::vector<std::string> tokens;

        while (std::getline(ss, item, ',')) {
            tokens.push_back(item);
        }

        if (tokens.size() >= 2) {
            float first = std::stof(tokens[0]);
            float second = std::stof(tokens[1]);
            data.emplace_back(std::make_pair(first, second));
        }
    }

    return data;
}

} // namespace CSVLoader
