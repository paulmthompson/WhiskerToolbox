#ifndef LINE_DATA_LMDB_HPP
#define LINE_DATA_LMDB_HPP

#include "Lines/Line_Data.hpp"

#include <memory>
#include <string>

// Save LineData to LMDB using Cap'n Proto
bool saveLineDataToLMDB(const LineData* lineData, const std::string& dbPath, const std::string& key = "default");

// Load LineData from LMDB using Cap'n Proto
std::shared_ptr<LineData> loadLineDataFromLMDB(const std::string& dbPath, const std::string& key = "default");

// Update specific time frames in an existing LMDB database
bool updateLineDataTimeFrames(const std::string& dbPath,
                              std::string& key,
                              std::map<int, std::vector<Line2D>>& timeFrames);

// Get specific time frames from LMDB without loading the entire dataset
std::map<int, std::vector<Line2D>> getLineDataTimeFrames(const std::string& dbPath,
                                                         std::string& key,
                                                         std::vector<int>& times);



#endif// LINE_DATA_LMDB_HPP
