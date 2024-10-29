#ifndef BEHAVIORTOOLBOX_CSV_LOADERS_HPP
#define BEHAVIORTOOLBOX_CSV_LOADERS_HPP

#include <string>
#include <vector>

namespace CSVLoader {

std::vector<float> loadSingleColumnCSV(const std::string& filename);

std::vector<std::pair<float, float>> loadPairColumnCSV(const std::string& filename);

} // namespace CSVLoader


#endif //BEHAVIORTOOLBOX_CSV_LOADERS_HPP
