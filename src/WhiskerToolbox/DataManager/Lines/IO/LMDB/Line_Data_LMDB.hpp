#ifndef LINE_DATA_LMDB_HPP
#define LINE_DATA_LMDB_HPP

#include "Lines/Line_Data.hpp"

#include <memory>
#include <string>

// Save LineData to LMDB using Cap'n Proto
bool saveLineDataToLMDB(LineData const * lineData, std::string const & dbPath, std::string const & key = "default");

// Load LineData from LMDB using Cap'n Proto
std::shared_ptr<LineData> loadLineDataFromLMDB(std::string const & dbPath, std::string const & key = "default");

// Update specific time frames in an existing LMDB database
bool updateLineDataTimeFrames(std::string const & dbPath,
                              std::string & key,
                              std::map<int, std::vector<Line2D>> & timeFrames);

// Get specific time frames from LMDB without loading the entire dataset
std::map<int, std::vector<Line2D>> getLineDataTimeFrames(std::string const & dbPath,
                                                         std::string & key,
                                                         std::vector<int> & times);


#endif// LINE_DATA_LMDB_HPP
