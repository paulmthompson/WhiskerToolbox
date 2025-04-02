#ifndef BEHAVIORTOOLBOX_CSV_LOADERS_HPP
#define BEHAVIORTOOLBOX_CSV_LOADERS_HPP

#include <string>
#include <vector>

namespace CSVLoader {

std::vector<float> loadSingleColumnCSV(std::string const & filename);

std::vector<std::pair<float, float>> loadPairColumnCSV(std::string const & filename);

}// namespace CSVLoader


#endif//BEHAVIORTOOLBOX_CSV_LOADERS_HPP
